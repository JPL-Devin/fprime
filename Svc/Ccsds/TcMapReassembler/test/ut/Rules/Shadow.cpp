// ======================================================================
// \title  Shadow.cpp
// \author thomas-bc-autobot
// \brief  Shadow-model step and invariants 1-7 (DESIGN §8.4) shared by every rule
// ======================================================================

#include <cstring>

#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/TcMapReassembler/test/ut/TcMapReassemblerTester.hpp"

namespace Svc {

namespace Ccsds {

namespace {

//! Raise an event in the shadow: returns 1 if the component must emit it (below its throttle)
U32 raise(U32& count, U32 throttle) {
    const U32 emitted = (count < throttle) ? 1 : 0;
    count++;
    return emitted;
}

}  // namespace

// ----------------------------------------------------------------------
// Rule helpers
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::poolFull() const {
    return (this->shadow_inProgressCount() + this->shadow_delivered.size() + this->m_held.size()) >= POOL_BUFFER_COUNT;
}

TcSequenceFlags::T TcMapReassemblerTester ::randomFlags() {
    switch (STest::Random::lowerUpper(0, 3)) {
        case 0:
            return TcSequenceFlags::CONTINUING;
        case 1:
            return TcSequenceFlags::FIRST;
        case 2:
            return TcSequenceFlags::LAST;
        default:
            return TcSequenceFlags::UNSEGMENTED;
    }
}

const U8* TcMapReassemblerTester ::randomSegment(FwSizeType len) {
    EXPECT_LE(len, this->m_scratch.size());
    for (FwSizeType i = 0; i < len; i++) {
        this->m_scratch[i] = static_cast<U8>(STest::Random::lowerUpper(0, 255));
    }
    return this->m_scratch.data();
}

void TcMapReassemblerTester ::planPacket(MapShadow& map, FwSizeType len) {
    TcMapReassemblerTester::buildPacket(map.planned, len);
}

FwSizeType TcMapReassemblerTester ::declaredOf(const std::vector<U8>& bytes) {
    if (bytes.size() < SP_HEADER_SIZE) {
        return 0;
    }
    return SP_HEADER_SIZE + ((static_cast<FwSizeType>(bytes[4]) << 8) | bytes[5]) + 1;
}

void TcMapReassemblerTester ::ruleSend(U8 mapId,
                                       TcSequenceFlags::T flags,
                                       const U8* data,
                                       FwSizeType len,
                                       bool present) {
    // ------------------------------------------------------------------
    // Predict from the shadow (DESIGN §5.2 / §5.3, written from the table, not from the code)
    // ------------------------------------------------------------------
    std::vector<FrameError::T> errors;
    U32 expAllocate = 0;
    U32 expDeallocate = 0;
    U32 expDataOut = 0;
    U32 expShAbsent = 0;
    U32 expInvalidMap = 0;
    U32 expUnexpected = 0;
    U32 expAbandoned = 0;
    U32 expEmpty = 0;
    U32 expTooLarge = 0;
    U32 expAllocFailed = 0;
    U32 expMismatch = 0;
    const U32 priorReassembled = this->shadow_packetsReassembled;
    const U32 priorDropped = this->shadow_segmentsDropped;
    const U32 priorAbandoned = this->shadow_packetsAbandoned;
    bool deliveredNow = false;

    MapShadow* map = nullptr;
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        if (this->shadow_maps[i].mapId == mapId) {
            map = &this->shadow_maps[i];
        }
    }

    // Local closures over the prediction state
    auto reject = [&](FrameError::T err) {
        errors.push_back(err);
        this->shadow_segmentsDropped++;
    };
    auto abandon = [&](FrameError::T err) {
        errors.push_back(err);
        expDeallocate++;
        this->shadow_packetsAbandoned++;
        map->inProgress = false;
        map->received.clear();
    };
    auto complete = [&]() {
        const FwSizeType declared = TcMapReassemblerTester::declaredOf(map->received);
        if (declared != map->received.size()) {
            expMismatch += raise(this->shadow_events.lengthMismatch, TcMapReassembler::EVENTID_LENGTHMISMATCH_THROTTLE);
            abandon(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
        } else {
            expDataOut++;
            this->shadow_packetsReassembled++;
            Delivered d;
            d.bytes = map->received;
            this->shadow_delivered.push_back(d);
            map->inProgress = false;
            map->received.clear();
            deliveredNow = true;
        }
    };

    if (not present) {  // P1
        expShAbsent +=
            raise(this->shadow_events.segmentHeaderAbsent, TcMapReassembler::EVENTID_SEGMENTHEADERABSENT_THROTTLE);
        reject(FrameError::TC_SEGMENT_HEADER_ABSENT);
    } else if (map == nullptr) {  // P2
        expInvalidMap += raise(this->shadow_events.invalidMapId, TcMapReassembler::EVENTID_INVALIDMAPID_THROTTLE);
        reject(FrameError::TC_INVALID_MAP_ID);
    } else if (len == 0) {  // P3
        expEmpty += raise(this->shadow_events.emptySegment, TcMapReassembler::EVENTID_EMPTYSEGMENT_THROTTLE);
        reject(FrameError::TC_SEGMENT_EMPTY);
    } else if ((flags == TcSequenceFlags::FIRST) || (flags == TcSequenceFlags::UNSEGMENTED)) {
        if (map->inProgress) {  // P4
            expAbandoned +=
                raise(this->shadow_events.packetAbandoned, TcMapReassembler::EVENTID_PACKETABANDONED_THROTTLE);
            abandon((flags == TcSequenceFlags::FIRST) ? FrameError::TC_SEGMENT_UNEXPECTED_FIRST
                                                      : FrameError::TC_SEGMENT_UNEXPECTED_UNSEGMENTED);
        }
        std::vector<U8> portion(data, data + len);
        if (len > MAX_PACKET_SIZE) {  // P6
            expTooLarge += raise(this->shadow_events.packetTooLarge, TcMapReassembler::EVENTID_PACKETTOOLARGE_THROTTLE);
            reject(FrameError::TC_SEGMENT_OVERFLOW);
        } else if ((len >= SP_HEADER_SIZE) && (TcMapReassemblerTester::declaredOf(portion) > MAX_PACKET_SIZE)) {  // P8
            expTooLarge += raise(this->shadow_events.packetTooLarge, TcMapReassembler::EVENTID_PACKETTOOLARGE_THROTTLE);
            reject(FrameError::TC_SEGMENT_OVERFLOW);
        } else {
            expAllocate++;
            if (this->poolFull()) {  // P7
                expAllocFailed +=
                    raise(this->shadow_events.allocationFailed, TcMapReassembler::EVENTID_ALLOCATIONFAILED_THROTTLE);
                reject(FrameError::TC_SEGMENT_ALLOC_FAILED);
            } else {  // P12
                map->inProgress = true;
                map->received = portion;
                if (flags == TcSequenceFlags::UNSEGMENTED) {  // P10 / P11
                    complete();
                }
            }
        }
    } else {
        if (not map->inProgress) {  // P5
            expUnexpected +=
                raise(this->shadow_events.unexpectedSegment, TcMapReassembler::EVENTID_UNEXPECTEDSEGMENT_THROTTLE);
            reject(FrameError::TC_SEGMENT_ORPHAN);
        } else if ((map->received.size() + len) > MAX_PACKET_SIZE) {  // P9
            expTooLarge += raise(this->shadow_events.packetTooLarge, TcMapReassembler::EVENTID_PACKETTOOLARGE_THROTTLE);
            abandon(FrameError::TC_SEGMENT_OVERFLOW);
        } else {  // P12
            map->received.insert(map->received.end(), data, data + len);
            if (flags == TcSequenceFlags::LAST) {  // P10 / P11
                complete();
            }
        }
    }

    // ------------------------------------------------------------------
    // Execute (sendSegment asserts the single synchronous frame return: invariant 4)
    // ------------------------------------------------------------------
    this->sendSegment(mapId, flags, data, len, present);
    this->shadow_dataInCalls++;

    // ------------------------------------------------------------------
    // Compare
    // ------------------------------------------------------------------
    ASSERT_from_allocate_SIZE(expAllocate);
    ASSERT_from_deallocate_SIZE(expDeallocate);
    ASSERT_from_dataOut_SIZE(expDataOut);
    ASSERT_from_errorNotify_SIZE(static_cast<U32>(errors.size()));
    for (FwSizeType i = 0; i < errors.size(); i++) {
        ASSERT_from_errorNotify(static_cast<U32>(i), FrameError(errors[i]));
    }
    ASSERT_EVENTS_SegmentHeaderAbsent_SIZE(expShAbsent);
    ASSERT_EVENTS_InvalidMapId_SIZE(expInvalidMap);
    ASSERT_EVENTS_UnexpectedSegment_SIZE(expUnexpected);
    ASSERT_EVENTS_PacketAbandoned_SIZE(expAbandoned);
    ASSERT_EVENTS_EmptySegment_SIZE(expEmpty);
    ASSERT_EVENTS_PacketTooLarge_SIZE(expTooLarge);
    ASSERT_EVENTS_AllocationFailed_SIZE(expAllocFailed);
    ASSERT_EVENTS_LengthMismatch_SIZE(expMismatch);
    ASSERT_EVENTS_SIZE(expShAbsent + expInvalidMap + expUnexpected + expAbandoned + expEmpty + expTooLarge +
                       expAllocFailed + expMismatch);
    if (expInvalidMap > 0) {
        ASSERT_EVENTS_InvalidMapId(0, mapId);
    }
    if (expUnexpected > 0) {
        ASSERT_EVENTS_UnexpectedSegment(0, mapId, flags);
    }
    if (expEmpty > 0) {
        ASSERT_EVENTS_EmptySegment(0, mapId, flags);
    }

    // Telemetry is written once per change (two abandons in one call write twice)
    const U32 deltaReassembled = this->shadow_packetsReassembled - priorReassembled;
    const U32 deltaDropped = this->shadow_segmentsDropped - priorDropped;
    const U32 deltaAbandoned = this->shadow_packetsAbandoned - priorAbandoned;
    ASSERT_TLM_PacketsReassembled_SIZE(deltaReassembled);
    if (deltaReassembled > 0) {
        ASSERT_TLM_PacketsReassembled(deltaReassembled - 1, this->shadow_packetsReassembled);
    }
    ASSERT_TLM_SegmentsDropped_SIZE(deltaDropped);
    if (deltaDropped > 0) {
        ASSERT_TLM_SegmentsDropped(deltaDropped - 1, this->shadow_segmentsDropped);
    }
    ASSERT_TLM_PacketsAbandoned_SIZE(deltaAbandoned);
    if (deltaAbandoned > 0) {
        ASSERT_TLM_PacketsAbandoned(deltaAbandoned - 1, this->shadow_packetsAbandoned);
    }

    // Invariant 5: delivered bytes and declared length
    if (deliveredNow) {
        Delivered& d = this->shadow_delivered.back();
        d.buffer = this->fromPortHistory_dataOut->at(0).data;
        ASSERT_TRUE(d.buffer.isValid());
        ASSERT_EQ(d.buffer.getSize(), d.bytes.size());
        ASSERT_EQ(::memcmp(d.buffer.getData(), d.bytes.data(), d.bytes.size()), 0);
        ASSERT_EQ(TcMapReassemblerTester::declaredOf(d.bytes), d.bytes.size());
        ASSERT_NE(d.buffer.getData(), this->m_frame.data());
        ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context, TcMapReassemblerTester::makeContext(flags, mapId));
    }
}

// ----------------------------------------------------------------------
// Invariants
// ----------------------------------------------------------------------

void TcMapReassemblerTester ::checkInvariants() {
    std::vector<const U8*> live;

    // Invariant 1: per-MAP state
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        const MapShadow& shadow = this->shadow_maps[i];
        TcMapReassembler::MapChannel* const ch = this->component.findMap(shadow.mapId);
        ASSERT_NE(ch, nullptr);
        ASSERT_LE(ch->received, MAX_PACKET_SIZE);
        ASSERT_EQ(ch->state == TcMapReassembler::MapChannel::IN_PROGRESS, ch->buffer.isValid());
        ASSERT_EQ(ch->state == TcMapReassembler::MapChannel::IN_PROGRESS, shadow.inProgress);
        ASSERT_EQ(ch->received, shadow.received.size());
        if (shadow.inProgress) {
            ASSERT_GE(ch->received, 1U);
            ASSERT_GE(ch->segments, 1U);
            ASSERT_EQ(ch->buffer.getSize(), MAX_PACKET_SIZE);
            ASSERT_EQ(::memcmp(ch->buffer.getData(), shadow.received.data(), shadow.received.size()), 0);
            live.push_back(ch->buffer.getData());
        } else {
            ASSERT_EQ(ch->segments, 0U);
        }
    }

    // Invariant 2: pool accounting; every live buffer is distinct
    for (const Delivered& d : this->shadow_delivered) {
        live.push_back(d.buffer.getData());
    }
    for (const Fw::Buffer& b : this->m_held) {
        live.push_back(b.getData());
    }
    ASSERT_EQ(this->m_poolAllocated, static_cast<U32>(live.size()));
    for (FwSizeType i = 0; i < live.size(); i++) {
        for (FwSizeType j = i + 1; j < live.size(); j++) {
            ASSERT_NE(live[i], live[j]);
        }
    }

    // Invariant 3 is enforced inside ruleSend: a rejected input leaves the shadow untouched, and
    // invariant 1 above shows the component agrees with the shadow.

    // Invariant 4: exactly one frame return per dataIn
    ASSERT_EQ(this->m_frameReturns, this->shadow_dataInCalls);

    // Invariant 5: delivered packets still carry their bytes until returned
    for (const Delivered& d : this->shadow_delivered) {
        ASSERT_EQ(d.buffer.getSize(), d.bytes.size());
        ASSERT_EQ(::memcmp(d.buffer.getData(), d.bytes.data(), d.bytes.size()), 0);
    }

    // Invariant 6: counters
    ASSERT_EQ(this->component.m_packetsReassembled, this->shadow_packetsReassembled);
    ASSERT_EQ(this->component.m_segmentsDropped, this->shadow_segmentsDropped);
    ASSERT_EQ(this->component.m_packetsAbandoned, this->shadow_packetsAbandoned);
    ASSERT_EQ(this->shadow_terminalOutcomes(),
              this->shadow_packetsReassembled + this->shadow_segmentsDropped + this->shadow_packetsAbandoned);

    // Invariant 7: the pool bound
    ASSERT_LE(this->m_poolAllocated, POOL_BUFFER_COUNT);
    ASSERT_LE(this->m_poolHighWater, POOL_BUFFER_COUNT);
    ASSERT_EQ(this->m_shortOutstanding, 0U);
}

}  // namespace Ccsds

}  // namespace Svc
