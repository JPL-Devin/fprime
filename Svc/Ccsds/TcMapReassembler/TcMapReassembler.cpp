// ======================================================================
// \title  TcMapReassembler.cpp
// \author thomas-bc-autobot
// \brief  cpp file for TcMapReassembler component implementation class
// ======================================================================

#include "Svc/Ccsds/TcMapReassembler/TcMapReassembler.hpp"

#include <cstring>

#include "Fw/Types/Assert.hpp"
#include "Svc/Ccsds/Utils/TcSegmentHeader.hpp"

namespace Svc {
namespace Ccsds {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TcMapReassembler ::TcMapReassembler(const char* const compName) : TcMapReassemblerComponentBase(compName) {}

TcMapReassembler ::~TcMapReassembler() {}

void TcMapReassembler ::configure(const U8* mapIds, FwSizeType count) {
    FW_ASSERT(mapIds != nullptr);
    FW_ASSERT(count >= 1 && count <= static_cast<FwSizeType>(TcMapCfg::MapChannelCount),
              static_cast<FwAssertArgType>(count));
    for (FwSizeType i = 0; i < count; i++) {
        FW_ASSERT(Utils::TcSegmentHeader::isValidMapId(mapIds[i]), static_cast<FwAssertArgType>(mapIds[i]));
        for (FwSizeType j = 0; j < i; j++) {
            FW_ASSERT(mapIds[j] != mapIds[i], static_cast<FwAssertArgType>(mapIds[i]));
        }
        this->m_maps[i].mapId = mapIds[i];
        this->m_maps[i].state = MapChannel::IDLE;
        this->m_maps[i].buffer = Fw::Buffer();
        this->m_maps[i].received = 0;
        this->m_maps[i].segments = 0;
    }
    this->m_mapCount = count;
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void TcMapReassembler ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // P1: fail closed when the upstream deframer did not strip a Segment Header (MF1)
    if (not context.get_tcSegmentHeaderPresent()) {
        this->log_WARNING_HI_SegmentHeaderAbsent();
        this->reject(FrameError::TC_SEGMENT_HEADER_ABSENT, data, context);
        return;
    }
    const U8 sh = context.get_tcSegmentHeader();
    const TcSequenceFlags::T flags = Utils::TcSegmentHeader::sequenceFlags(sh);
    const U8 mapId = Utils::TcSegmentHeader::mapId(sh);

    // P2: MAP demultiplexing, unrecognised MAP discarded (232.0-B-4 4.4.3.3)
    MapChannel* const ch = this->findMap(mapId);
    if (ch == nullptr) {
        this->log_WARNING_LO_InvalidMapId(mapId);
        this->reject(FrameError::TC_INVALID_MAP_ID, data, context);
        return;
    }

    // P3: a segment must carry at least one User Data octet
    const FwSizeType portion = data.getSize();
    if (portion == 0) {
        this->log_WARNING_LO_EmptySegment(mapId, flags);
        this->reject(FrameError::TC_SEGMENT_EMPTY, data, context);
        return;
    }

    switch (flags) {
        case TcSequenceFlags::FIRST:
        case TcSequenceFlags::UNSEGMENTED:
            // P4: a new packet supersedes the partial one on this MAP
            if (ch->state == MapChannel::IN_PROGRESS) {
                this->log_WARNING_HI_PacketAbandoned(mapId, static_cast<U32>(ch->received), flags);
                this->abandon(*ch, (flags == TcSequenceFlags::FIRST) ? FrameError::TC_SEGMENT_UNEXPECTED_FIRST
                                                                     : FrameError::TC_SEGMENT_UNEXPECTED_UNSEGMENTED);
            }
            if (this->startPacket(*ch, data, context)) {
                // Frame returned before any downstream call
                this->dataReturnOut_out(0, data, context);
                if (flags == TcSequenceFlags::UNSEGMENTED) {
                    this->complete(*ch, context);
                }
            }
            return;
        case TcSequenceFlags::CONTINUING:
        case TcSequenceFlags::LAST:
            // P5: nothing to continue
            if (ch->state == MapChannel::IDLE) {
                this->log_WARNING_HI_UnexpectedSegment(mapId, flags);
                this->reject(FrameError::TC_SEGMENT_ORPHAN, data, context);
                return;
            }
            // P9: accumulated overflow discards the partial packet
            if ((ch->received + portion) > static_cast<FwSizeType>(TcMapCfg::MaxPacketSize)) {
                this->log_WARNING_HI_PacketTooLarge(mapId, static_cast<U32>(ch->received + portion),
                                                    static_cast<U32>(TcMapCfg::MaxPacketSize));
                this->abandon(*ch, FrameError::TC_SEGMENT_OVERFLOW);
                this->dataReturnOut_out(0, data, context);
                return;
            }
            // P12: append
            (void)::memcpy(ch->buffer.getData() + ch->received, data.getData(), portion);
            ch->received += portion;
            ch->segments++;
            this->dataReturnOut_out(0, data, context);
            if (flags == TcSequenceFlags::LAST) {
                this->complete(*ch, context);
            }
            return;
        default:
            // Two-bit field: every value is a defined flag
            FW_ASSERT(0, static_cast<FwAssertArgType>(flags));
            break;
    }
}

void TcMapReassembler ::dataReturnIn_handler(FwIndexType portNum,
                                             Fw::Buffer& data,
                                             const ComCfg::FrameContext& context) {
    // Every buffer that left on dataOut was allocated from the dedicated pool by this component
    this->deallocate_out(0, data);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

TcMapReassembler::MapChannel* TcMapReassembler ::findMap(U8 mapId) {
    for (FwSizeType i = 0; i < this->m_mapCount; i++) {
        if (this->m_maps[i].mapId == mapId) {
            return &this->m_maps[i];
        }
    }
    return nullptr;
}

bool TcMapReassembler ::startPacket(MapChannel& ch, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    FW_ASSERT(ch.state == MapChannel::IDLE, static_cast<FwAssertArgType>(ch.state));
    const FwSizeType portion = data.getSize();
    const FwSizeType maxSize = static_cast<FwSizeType>(TcMapCfg::MaxPacketSize);

    // P6: a single segment larger than any accepted packet
    if (portion > maxSize) {
        this->log_WARNING_HI_PacketTooLarge(ch.mapId, static_cast<U32>(portion), static_cast<U32>(maxSize));
        this->reject(FrameError::TC_SEGMENT_OVERFLOW, data, context);
        return false;
    }
    // P8: the Space Packet header already declares a length above the bound; checked before allocating
    if (portion >= static_cast<FwSizeType>(TcMapCfg::SpacePacketHeaderSize)) {
        const FwSizeType declared = TcMapReassembler::declaredLength(data.getData());
        if (declared > maxSize) {
            this->log_WARNING_HI_PacketTooLarge(ch.mapId, static_cast<U32>(declared), static_cast<U32>(maxSize));
            this->reject(FrameError::TC_SEGMENT_OVERFLOW, data, context);
            return false;
        }
    }
    // P7: the pool must hand out a valid buffer of at least the requested size (MF6)
    Fw::Buffer buffer = this->allocate_out(0, static_cast<FwSizeType>(TcMapCfg::MaxPacketSize));
    if ((not buffer.isValid()) || (buffer.getSize() < maxSize)) {
        if (buffer.isValid()) {
            this->deallocate_out(0, buffer);
        }
        this->log_WARNING_HI_AllocationFailed(ch.mapId, static_cast<U32>(maxSize));
        this->reject(FrameError::TC_SEGMENT_ALLOC_FAILED, data, context);
        return false;
    }
    // P12: copy the first portion into the pool buffer
    (void)::memcpy(buffer.getData(), data.getData(), portion);
    ch.buffer = buffer;
    ch.received = portion;
    ch.segments = 1;
    ch.state = MapChannel::IN_PROGRESS;
    return true;
}

void TcMapReassembler ::complete(MapChannel& ch, const ComCfg::FrameContext& context) {
    FW_ASSERT(ch.state == MapChannel::IN_PROGRESS, static_cast<FwAssertArgType>(ch.state));
    // P10: one Space Packet per Frame Data Unit, exact length (133.0-B-2 4.1.3.5; no blocking)
    const FwSizeType declared = (ch.received >= static_cast<FwSizeType>(TcMapCfg::SpacePacketHeaderSize))
                                    ? TcMapReassembler::declaredLength(ch.buffer.getData())
                                    : 0;
    if (declared != ch.received) {
        this->log_WARNING_HI_LengthMismatch(ch.mapId, static_cast<U32>(ch.received), static_cast<U32>(declared));
        this->abandon(ch, FrameError::TC_SEGMENT_LENGTH_MISMATCH);
        return;
    }
    // P11: MAP goes IDLE before the buffer leaves, so a synchronous return cannot race the state
    Fw::Buffer out = ch.buffer;
    out.setSize(ch.received);
    ch.buffer = Fw::Buffer();
    ch.received = 0;
    ch.segments = 0;
    ch.state = MapChannel::IDLE;
    this->m_packetsReassembled++;
    this->tlmWrite_PacketsReassembled(this->m_packetsReassembled);
    this->dataOut_out(0, out, context);
}

void TcMapReassembler ::abandon(MapChannel& ch, FrameError::T err) {
    FW_ASSERT(ch.state == MapChannel::IN_PROGRESS, static_cast<FwAssertArgType>(ch.state));
    this->deallocate_out(0, ch.buffer);
    ch.buffer = Fw::Buffer();
    ch.received = 0;
    ch.segments = 0;
    ch.state = MapChannel::IDLE;
    this->m_packetsAbandoned++;
    this->tlmWrite_PacketsAbandoned(this->m_packetsAbandoned);
    this->notifyError(err);
}

void TcMapReassembler ::reject(FrameError::T err, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->m_segmentsDropped++;
    this->tlmWrite_SegmentsDropped(this->m_segmentsDropped);
    this->notifyError(err);
    this->dataReturnOut_out(0, data, context);
}

void TcMapReassembler ::notifyError(FrameError::T err) {
    if (this->isConnected_errorNotify_OutputPort(0)) {
        this->errorNotify_out(0, err);
    }
}

FwSizeType TcMapReassembler ::declaredLength(const U8* const p) {
    FW_ASSERT(p != nullptr);
    const U16 lenField = static_cast<U16>((static_cast<U16>(p[4]) << 8) | p[5]);
    return static_cast<FwSizeType>(TcMapCfg::SpacePacketHeaderSize) + static_cast<FwSizeType>(lenField) + 1;
}

}  // namespace Ccsds
}  // namespace Svc
