// ======================================================================
// \title  SppZonePacker.cpp
// \author devin
// \brief  cpp file for SppZonePacker component implementation class
// ======================================================================

#include "Svc/Ccsds/SppZonePacker/SppZonePacker.hpp"

#include "Svc/Ccsds/Types/FppConstantsAc.hpp"

namespace Svc {

namespace Ccsds {

// Out-of-line definitions for odr-used constants (C++14)
constexpr U8 SppZonePacker::SPP_IDLE_DATA_PATTERN;
constexpr U8 SppZonePacker::MIN_SPP_LENGTH;
constexpr FwSizeType SppZonePacker::POOL_DEPTH;
constexpr U8 SppZonePacker::MAX_CREDIT_OWED;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SppZonePacker ::SppZonePacker(const char* const compName) : SppZonePackerComponentBase(compName) {}

SppZonePacker ::~SppZonePacker() {}

void SppZonePacker ::configure(FwSizeType zoneSize, FwSizeType headroom, FwSizeType trailerReserve, U8 vcId) {
    // The zone plus its reserves must fit in the compile-time backing storage
    FW_ASSERT(headroom + zoneSize + trailerReserve <= ComCfg::MaxTransferFrameSize,
              static_cast<FwAssertArgType>(headroom), static_cast<FwAssertArgType>(zoneSize),
              static_cast<FwAssertArgType>(trailerReserve));
    // The zone must hold at least a minimum-size Space Packet
    FW_ASSERT(zoneSize >= MIN_SPP_LENGTH, static_cast<FwAssertArgType>(zoneSize));
    // Zone byte offsets must be encodable below the canonical FHP sentinel values
    FW_ASSERT(zoneSize < ComCfg::FhpValues::FHP_IDLE_DATA_ONLY, static_cast<FwAssertArgType>(zoneSize));
    // Virtual Channel ID is at most 6 bits across TM/AOS/USLP
    FW_ASSERT((vcId & 0xC0) == 0, static_cast<FwAssertArgType>(vcId));

    this->m_zoneSize = zoneSize;
    this->m_headroom = headroom;
    this->m_trailerReserve = trailerReserve;

    for (FwSizeType i = 0; i < sizeof(this->m_vcs) / sizeof(this->m_vcs[0]); i++) {
        ZoneVc& vc = this->m_vcs[i];
        vc.configured = true;
        vc.virtualChannelId = vcId;
        vc.lastContext = ComCfg::FrameContext();
        vc.lastContext.set_vcId(vcId);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SppZonePacker ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    FW_ASSERT(data.getSize() > 0);
    Emission em;
    this->m_mutex.lock();
    ZoneVc& vc = this->getVc(context);

    // The Communication Adapter Interface guarantees at most one packet in flight
    // per credit: a packet cannot arrive while a fragment is still outstanding or
    // while the zone under construction is full and waiting to be sent
    FW_ASSERT(!vc.outstanding.packet.isValid());
    FW_ASSERT(!vc.sendPendingFull);
    FW_ASSERT(vc.payloadOffset < this->m_zoneSize, static_cast<FwAssertArgType>(vc.payloadOffset));

    // This is a fresh packet: record the First Header Pointer if it is the first in the zone
    if (!vc.pastFirstFreshPacket) {
        vc.fhp = vc.payloadOffset;
        vc.pastFirstFreshPacket = true;
    }
    this->m_packetsPacked++;

    this->packBytes(vc, data, context, 0, false, em);

    // Idle-fill and send the zone right away when requested
    if (context.get_sendNow() && (vc.payloadOffset < this->m_zoneSize) && !vc.outstanding.packet.isValid()) {
        this->fillWithIdle(vc, em);
    }

    this->finishPacking(vc, em);

    if (vc.flushPending) {
        this->tryFlush(vc, em);
    }
    this->snapshotTelemetry(em);
    this->m_mutex.unlock();
    this->emit(em);
}

void SppZonePacker ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Emission em;
    this->m_mutex.lock();
    ZoneVc& vc = this->getVc(context);

    // Identify and reclaim the returned pool entry
    FwIndexType poolIndex = -1;
    for (FwIndexType i = 0; i < static_cast<FwIndexType>(POOL_DEPTH); i++) {
        if (bufferBelongs(data, vc.pool[i].backer, sizeof(vc.pool[i].backer))) {
            poolIndex = i;
            break;
        }
    }
    FW_ASSERT(poolIndex >= 0);
    FW_ASSERT(vc.pool[poolIndex].state == BufferOwnershipState::NOT_OWNED,
              static_cast<FwAssertArgType>(vc.pool[poolIndex].state));
    vc.pool[poolIndex].state = BufferOwnershipState::OWNED;

    if (vc.sendPendingFull) {
        // A full zone was waiting for the in-flight zone to return: send it now
        this->prepareZoneSend(vc, em);
    }

    // Continue packing an outstanding fragment into the (fresh) zone under construction
    if (vc.outstanding.packet.isValid() && (vc.payloadOffset < this->m_zoneSize) &&
        (vc.pool[vc.fillIndex].state == BufferOwnershipState::OWNED)) {
        Fw::Buffer packet = vc.outstanding.packet;
        const ComCfg::FrameContext packetContext = vc.outstanding.context;
        const FwSizeType offset = vc.outstanding.offset;
        const bool isIdle = vc.outstanding.isIdle;

        this->packBytes(vc, packet, packetContext, offset, isIdle, em);

        if (packetContext.get_sendNow() && (vc.payloadOffset < this->m_zoneSize) && !vc.outstanding.packet.isValid()) {
            this->fillWithIdle(vc, em);
        }

        this->finishPacking(vc, em);
    }

    if (vc.packetCreditPending && !vc.sendPendingFull && (vc.pool[vc.fillIndex].state == BufferOwnershipState::OWNED)) {
        // A packet that exactly filled a zone is credited once the pipeline can
        // accept the next packet (fill buffer owned and not full)
        FW_ASSERT(!em.sendStatus);
        em.sendStatus = true;
        em.status = Fw::Success::SUCCESS;
        vc.packetCreditPending = false;
    }

    if (vc.flushPending) {
        this->tryFlush(vc, em);
    }
    this->snapshotTelemetry(em);
    this->m_mutex.unlock();
    this->emit(em);
}

void SppZonePacker ::comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) {
    Emission em;
    this->m_mutex.lock();
    ZoneVc& vc = this->m_vcs[0];
    FW_ASSERT(vc.configured);

    if (condition == Fw::Success::FAILURE) {
        // FAILURE always passes through so ComQueue enters retry; pending credits are void
        vc.creditOwed = 0;
        vc.packetCreditPending = false;
        em.sendStatus = true;
        em.status = Fw::Success::FAILURE;
    } else if (vc.creditOwed > 0) {
        // Absorb the SUCCESS for a zone this component emitted on its own credit
        vc.creditOwed--;
    } else {
        // Initial or recovery SUCCESS: pass through to prime/resume ComQueue
        em.sendStatus = true;
        em.status = Fw::Success::SUCCESS;
    }

    if (vc.flushPending) {
        this->tryFlush(vc, em);
    }
    this->snapshotTelemetry(em);
    this->m_mutex.unlock();
    this->emit(em);
}

void SppZonePacker ::run_handler(FwIndexType portNum, U32 context) {
    Emission em;
    this->m_mutex.lock();
    ZoneVc& vc = this->m_vcs[0];
    FW_ASSERT(vc.configured);
    this->tryFlush(vc, em);
    this->snapshotTelemetry(em);
    this->m_mutex.unlock();
    this->emit(em);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

SppZonePacker::ZoneVc& SppZonePacker ::getVc(const ComCfg::FrameContext& context) {
    // Multi-VC support would look up the VC struct by context.vcId; single VC for now
    ZoneVc& vc = this->m_vcs[0];
    FW_ASSERT(vc.configured);
    FW_ASSERT(vc.virtualChannelId == context.get_vcId(), static_cast<FwAssertArgType>(context.get_vcId()));
    return vc;
}

Fw::Buffer SppZonePacker ::zoneWindow(ZoneVc& vc) {
    PoolEntry& entry = vc.pool[vc.fillIndex];
    Fw::Buffer zone(entry.backer, this->m_headroom + this->m_zoneSize + this->m_trailerReserve);
    zone.advance(static_cast<FwSignedSizeType>(this->m_headroom));
    zone.setSize(this->m_zoneSize);
    return zone;
}

void SppZonePacker ::packBytes(ZoneVc& vc,
                               Fw::Buffer& data,
                               const ComCfg::FrameContext& context,
                               FwSizeType dataOffset,
                               bool isIdle,
                               Emission& em) {
    const FwSizeType bytesAvailable = this->m_zoneSize - vc.payloadOffset;
    FW_ASSERT(bytesAvailable > 0);
    FW_ASSERT(dataOffset < data.getSize(), static_cast<FwAssertArgType>(dataOffset),
              static_cast<FwAssertArgType>(data.getSize()));

    Fw::Buffer zone = this->zoneWindow(vc);
    auto zoneSerializer = zone.getSerializer();
    Fw::SerializeStatus status = zoneSerializer.moveSerToOffset(vc.payloadOffset);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    const U8* dataStart = data.getData() + dataOffset;
    FwSizeType dataSize = data.getSize() - dataOffset;

    if (dataSize <= bytesAvailable) {
        // The remaining packet bytes fit entirely in this zone
        vc.outstanding.offset = 0;
    } else {
        // Clamp to the zone boundary and hold the rest as the outstanding fragment
        dataSize = bytesAvailable;
        vc.outstanding.offset = dataOffset + dataSize;
        vc.outstanding.packet = data;
        vc.outstanding.isIdle = isIdle;
    }

    status = zoneSerializer.serializeFrom(dataStart, dataSize, Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    vc.payloadOffset = static_cast<U16>(vc.payloadOffset + dataSize);
    vc.realDataInZone = vc.realDataInZone || !isIdle;
    vc.outstanding.context = context;
    vc.lastContext = context;

    if (vc.outstanding.offset == 0) {
        // Packet fully consumed: return it upstream (idle packets are member-backed)
        if (!isIdle) {
            FW_ASSERT(!em.returnPacket);
            em.returnPacket = true;
            em.packet = data;
            em.packetContext = context;
        }
        vc.outstanding.packet = Fw::Buffer();
        vc.outstanding.isIdle = false;
    }
}

void SppZonePacker ::finishPacking(ZoneVc& vc, Emission& em) {
    FW_ASSERT(vc.payloadOffset <= this->m_zoneSize, static_cast<FwAssertArgType>(vc.payloadOffset));
    if (vc.payloadOffset == this->m_zoneSize) {
        // Zone full: send it, or defer while another zone is still in flight
        const FwIndexType otherIndex = (vc.fillIndex == 0) ? 1 : 0;
        if (vc.pool[otherIndex].state == BufferOwnershipState::NOT_OWNED) {
            vc.sendPendingFull = true;
        } else {
            this->prepareZoneSend(vc, em);
        }
        if (em.returnPacket) {
            // Packet consumed exactly at the zone boundary: credit on zone return
            vc.packetCreditPending = true;
        }
    } else if (em.returnPacket) {
        // Packet consumed with zone room to spare: credit ComQueue for the next packet
        FW_ASSERT(!em.sendStatus);
        em.sendStatus = true;
        em.status = Fw::Success::SUCCESS;
    }
}

void SppZonePacker ::prepareZoneSend(ZoneVc& vc, Emission& em) {
    FW_ASSERT(!em.sendZone);
    PoolEntry& entry = vc.pool[vc.fillIndex];
    FW_ASSERT(entry.state == BufferOwnershipState::OWNED, static_cast<FwAssertArgType>(entry.state));

    em.sendZone = true;
    em.zone = this->zoneWindow(vc);
    em.zoneContext = vc.lastContext;
    // An idle-only zone carries the idle-data sentinel; a pure continuation zone the
    // no-packet-start sentinel; otherwise the offset of the first packet header
    U16 fhp = static_cast<U16>(ComCfg::FhpValues::FHP_NO_PACKET_START);
    if (!vc.realDataInZone) {
        fhp = static_cast<U16>(ComCfg::FhpValues::FHP_IDLE_DATA_ONLY);
    } else if (vc.pastFirstFreshPacket) {
        fhp = vc.fhp;
    }
    em.zoneContext.set_firstHeaderPointer(fhp);
    em.zoneContext.set_zeroCopyFrame(true);
    em.zoneContext.set_sendNow(false);

    entry.state = BufferOwnershipState::NOT_OWNED;
    vc.creditOwed++;
    FW_ASSERT(vc.creditOwed <= MAX_CREDIT_OWED, static_cast<FwAssertArgType>(vc.creditOwed));

    vc.fillIndex = (vc.fillIndex == 0) ? 1 : 0;
    vc.payloadOffset = 0;
    vc.pastFirstFreshPacket = false;
    vc.fhp = 0;
    vc.realDataInZone = false;
    vc.sendPendingFull = false;

    this->m_zonesSent++;
}

void SppZonePacker ::fillWithIdle(ZoneVc& vc, Emission& em) {
    FW_ASSERT(vc.payloadOffset < this->m_zoneSize, static_cast<FwAssertArgType>(vc.payloadOffset));
    const U16 idleSize = static_cast<U16>(this->m_zoneSize - vc.payloadOffset);

    // The idle packet header is a fresh packet header for FHP purposes
    if (!vc.pastFirstFreshPacket) {
        vc.fhp = vc.payloadOffset;
        vc.pastFirstFreshPacket = true;
    }

    if (idleSize < MIN_SPP_LENGTH) {
        // Too few bytes for a minimum Space Packet: stripe one across the zone boundary
        FW_ASSERT(!vc.outstanding.packet.isValid());
        Fw::Buffer idle(vc.sppIdleBacker, MIN_SPP_LENGTH);
        auto idleSerializer = idle.getSerializer();
        serializeIdlePacket(idleSerializer, MIN_SPP_LENGTH);
        this->packBytes(vc, idle, vc.lastContext, 0, true, em);
        this->m_idleBytesSent += MIN_SPP_LENGTH;
    } else {
        Fw::Buffer zone = this->zoneWindow(vc);
        auto zoneSerializer = zone.getSerializer();
        Fw::SerializeStatus status = zoneSerializer.moveSerToOffset(vc.payloadOffset);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        serializeIdlePacket(zoneSerializer, idleSize);
        vc.payloadOffset = static_cast<U16>(this->m_zoneSize);
        this->m_idleBytesSent += idleSize;
    }
}

void SppZonePacker ::tryFlush(ZoneVc& vc, Emission& em) {
    if ((vc.payloadOffset == 0) && !vc.outstanding.packet.isValid()) {
        // Nothing pending: nothing to flush
        vc.flushPending = false;
        return;
    }
    if (!this->isQuiescent(vc) || vc.outstanding.packet.isValid() || vc.sendPendingFull || em.sendZone) {
        // A zone is in flight or uncredited: defer until the pipeline settles
        vc.flushPending = true;
        return;
    }
    const U16 idleBytes = static_cast<U16>(this->m_zoneSize - vc.payloadOffset);
    this->fillWithIdle(vc, em);
    FW_ASSERT(vc.payloadOffset == this->m_zoneSize, static_cast<FwAssertArgType>(vc.payloadOffset));
    this->prepareZoneSend(vc, em);
    vc.flushPending = false;
    em.flushed = true;
    em.flushIdleBytes = idleBytes;
}

bool SppZonePacker ::isQuiescent(const ZoneVc& vc) const {
    bool allOwned = true;
    for (FwSizeType i = 0; i < POOL_DEPTH; i++) {
        allOwned = allOwned && (vc.pool[i].state == BufferOwnershipState::OWNED);
    }
    return allOwned && (vc.creditOwed == 0);
}

void SppZonePacker ::serializeIdlePacket(Fw::SerialBufferBase& serializer, U16 length) {
    // APID to use for this Idle Packet
    constexpr U16 idleApid = static_cast<U16>(ComCfg::Apid::SPP_IDLE_PACKET);

    // Length token is defined as the number of bytes of payload data minus 1
    const U16 lengthToken = static_cast<U16>(length - SpacePacketHeader::SERIALIZED_SIZE - 1);

    SpacePacketHeader header;
    header.set_packetIdentification(idleApid);
    // Sequence Flags = 0b11 (unsegmented) & unused sequence count
    header.set_packetSequenceControl(0x3 << SpacePacketSubfields::SeqFlagsOffset);
    header.set_packetDataLength(lengthToken);
    Fw::SerializeStatus status = serializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    for (U16 i = static_cast<U16>(SpacePacketHeader::SERIALIZED_SIZE); i < length; i++) {
        status = serializer.serializeFrom(SPP_IDLE_DATA_PATTERN);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }
}

bool SppZonePacker ::bufferBelongs(const Fw::Buffer& buffer, const U8* start, FwSizeType size) {
    return (buffer.getData() >= start) && (buffer.getData() < (start + size));
}

void SppZonePacker ::snapshotTelemetry(Emission& em) {
    em.zonesSent = this->m_zonesSent;
    em.packetsPacked = this->m_packetsPacked;
    em.idleBytesSent = this->m_idleBytesSent;
}

void SppZonePacker ::emit(const Emission& em) {
    if (em.sendZone) {
        Fw::Buffer zone = em.zone;
        ComCfg::FrameContext context = em.zoneContext;
        this->dataOut_out(0, zone, context);
    }
    if (em.returnPacket) {
        Fw::Buffer packet = em.packet;
        ComCfg::FrameContext context = em.packetContext;
        this->dataReturnOut_out(0, packet, context);
    }
    if (em.sendStatus && this->isConnected_comStatusOut_OutputPort(0)) {
        Fw::Success status = em.status;
        this->comStatusOut_out(0, status);
    }
    if (em.flushed) {
        this->log_DIAGNOSTIC_ZoneFlushed(em.flushIdleBytes);
    }
    this->tlmWrite_ZonesSent(em.zonesSent);
    this->tlmWrite_PacketsPacked(em.packetsPacked);
    this->tlmWrite_IdleBytesSent(em.idleBytesSent);
}

}  // namespace Ccsds

}  // namespace Svc
