// ======================================================================
// \title  TcMapReassemblerTester.cpp
// \author thomas-bc-autobot
// \brief  cpp file for TcMapReassembler component test harness implementation class
// ======================================================================

#include "Svc/Ccsds/TcMapReassembler/test/ut/TcMapReassemblerTester.hpp"

#include <cstring>

#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/Utils/TcSegmentHeader.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

// C++14: ODR-used static const members need a definition
const FwSizeType TcMapReassemblerTester::MAX_HISTORY_SIZE;
const FwEnumStoreType TcMapReassemblerTester::TEST_INSTANCE_ID;
const FwEnumStoreType TcMapReassemblerTester::POOL_MEM_ID;
const U8 TcMapReassemblerTester::TEST_VC_ID;

TcMapReassemblerTester ::TcMapReassemblerTester()
    : TcMapReassemblerGTestBase("TcMapReassemblerTester", TcMapReassemblerTester::MAX_HISTORY_SIZE),
      component("TcMapReassembler"),
      m_pool("TcPacketBufferManager"),
      m_frame(MAX_PACKET_SIZE + 1, 0),
      m_short(MAX_PACKET_SIZE, 0),
      m_scratch(MAX_PACKET_SIZE + 1, 0) {
    this->initComponents();
    this->connectPorts();

    // Real pool sized exactly as the design's dedicated BufferManager (DESIGN §6.1); its event,
    // telemetry and time ports stay unconnected on purpose
    this->m_pool.init(TEST_INSTANCE_ID);
    Svc::BufferManager::BufferBins bins;
    (void)::memset(&bins, 0, sizeof(bins));
    bins.bins[0].bufferSize = static_cast<Fw::Buffer::SizeType>(MAX_PACKET_SIZE);
    bins.bins[0].numBuffers = static_cast<U16>(POOL_BUFFER_COUNT);
    this->m_pool.setup(static_cast<U16>(TcMapCfg::PoolManagerId), POOL_MEM_ID, this->m_allocator, bins);

    this->configureDefault();
}

TcMapReassemblerTester ::~TcMapReassemblerTester() {
    this->releaseHeldBuffers();
    this->m_pool.cleanup();
}

// ----------------------------------------------------------------------
// Handler overrides for typed from ports
// ----------------------------------------------------------------------

Fw::Buffer TcMapReassemblerTester ::from_allocate_handler(FwIndexType portNum, FwSizeType size) {
    this->pushFromPortEntry_allocate(size);
    switch (this->m_allocMode) {
        case ALLOC_INVALID:
            return Fw::Buffer();
        case ALLOC_SHORT: {
            EXPECT_GE(size, static_cast<FwSizeType>(1));
            Fw::Buffer shortBuffer(this->m_short.data(), static_cast<Fw::Buffer::SizeType>(size - 1));
            this->m_shortOutstanding++;
            return shortBuffer;
        }
        default: {
            Fw::Buffer buffer = this->m_pool.get_bufferGetCallee_InputPort(0)->invoke(size);
            if (buffer.isValid()) {
                this->m_poolAllocated++;
                if (this->m_poolAllocated > this->m_poolHighWater) {
                    this->m_poolHighWater = this->m_poolAllocated;
                }
            }
            return buffer;
        }
    }
}

void TcMapReassemblerTester ::from_deallocate_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->pushFromPortEntry_deallocate(fwBuffer);
    if (fwBuffer.getData() == this->m_short.data()) {
        EXPECT_GT(this->m_shortOutstanding, 0U) << "short stub buffer deallocated more than once";
        this->m_shortOutstanding--;
        this->m_shortDeallocated++;
        return;
    }
    ASSERT_GT(this->m_poolAllocated, 0U) << "deallocate of a buffer the pool does not hold out";
    this->m_poolAllocated--;
    this->m_pool.get_bufferSendIn_InputPort(0)->invoke(fwBuffer);
}

void TcMapReassemblerTester ::from_dataReturnOut_handler(FwIndexType portNum,
                                                         Fw::Buffer& data,
                                                         const ComCfg::FrameContext& context) {
    this->pushFromPortEntry_dataReturnOut(data, context);
    this->m_frameReturns++;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void TcMapReassemblerTester ::configureDefault() {
    U8 mapIds[MAP_CHANNEL_COUNT];
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        mapIds[i] = TcMapReassemblerTester::testMapId(i);
        this->shadow_maps[i].mapId = mapIds[i];
    }
    this->component.configure(mapIds, MAP_CHANNEL_COUNT);
}

U8 TcMapReassemblerTester ::testMapId(FwSizeType i) {
    // {0, 5, 10, ...}: gcd(5, 64) == 1 so the 64 first values are pairwise distinct
    return static_cast<U8>((i * 5) % 64);
}

ComCfg::FrameContext TcMapReassemblerTester ::makeContext(TcSequenceFlags::T flags, U8 mapId, bool present) {
    ComCfg::FrameContext context;
    context.set_vcId(TEST_VC_ID);
    context.set_tcSegmentHeaderPresent(present);
    context.set_tcSegmentHeader(Utils::TcSegmentHeader::encode(flags, mapId));
    return context;
}

void TcMapReassemblerTester ::buildPacket(std::vector<U8>& pkt, FwSizeType len, FwSizeType declared) {
    pkt.resize(len);
    for (FwSizeType i = 0; i < len; i++) {
        pkt[i] = static_cast<U8>(STest::Random::lowerUpper(0, 255));
    }
    if (len >= SP_HEADER_SIZE) {
        // 133.0-B-2 4.1.3.5: Packet Data Length = total - 7
        ASSERT_GE(declared, SP_MIN_SIZE);
        const FwSizeType field = declared - SP_HEADER_SIZE - 1;
        ASSERT_LE(field, static_cast<FwSizeType>(0xFFFF));
        pkt[4] = static_cast<U8>((field >> 8) & 0xFF);
        pkt[5] = static_cast<U8>(field & 0xFF);
    }
}

void TcMapReassemblerTester ::buildPacket(std::vector<U8>& pkt, FwSizeType len) {
    TcMapReassemblerTester::buildPacket(pkt, len, len);
}

void TcMapReassemblerTester ::sendSegment(U8 mapId,
                                          TcSequenceFlags::T flags,
                                          const U8* data,
                                          FwSizeType len,
                                          bool present) {
    ASSERT_LE(len, this->m_frame.size());
    this->clearHistory();
    if (len > 0) {
        (void)::memcpy(this->m_frame.data(), data, len);
    }
    Fw::Buffer frame(this->m_frame.data(), static_cast<Fw::Buffer::SizeType>(len));
    const ComCfg::FrameContext context = TcMapReassemblerTester::makeContext(flags, mapId, present);
    this->invoke_to_dataIn(0, frame, context);
    // Invariant 4: exactly one synchronous frame return carrying the same buffer
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), this->m_frame.data());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), len);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context, context);
}

void TcMapReassemblerTester ::sendPortion(U8 mapId,
                                          TcSequenceFlags::T flags,
                                          const std::vector<U8>& pkt,
                                          FwSizeType offset,
                                          FwSizeType len) {
    ASSERT_LE(offset + len, pkt.size());
    this->sendSegment(mapId, flags, pkt.data() + offset, len);
}

void TcMapReassemblerTester ::returnDelivered(FwSizeType index) {
    ASSERT_GT(this->fromPortHistory_dataOut->size(), index);
    Fw::Buffer buffer = this->fromPortHistory_dataOut->at(static_cast<U32>(index)).data;
    const ComCfg::FrameContext context = this->fromPortHistory_dataOut->at(static_cast<U32>(index)).context;
    this->invoke_to_dataReturnIn(0, buffer, context);
}

void TcMapReassemblerTester ::assertIdle(U8 mapId) {
    TcMapReassembler::MapChannel* const ch = this->component.findMap(mapId);
    ASSERT_NE(ch, nullptr);
    ASSERT_EQ(ch->state, TcMapReassembler::MapChannel::IDLE);
    ASSERT_FALSE(ch->buffer.isValid());
    ASSERT_EQ(ch->received, 0U);
    ASSERT_EQ(ch->segments, 0U);
}

void TcMapReassemblerTester ::assertInProgress(U8 mapId, FwSizeType received) {
    TcMapReassembler::MapChannel* const ch = this->component.findMap(mapId);
    ASSERT_NE(ch, nullptr);
    ASSERT_EQ(ch->state, TcMapReassembler::MapChannel::IN_PROGRESS);
    ASSERT_TRUE(ch->buffer.isValid());
    ASSERT_EQ(ch->buffer.getSize(), MAX_PACKET_SIZE);
    ASSERT_EQ(ch->received, received);
}

void TcMapReassemblerTester ::assertDelivered(FwSizeType index, const std::vector<U8>& pkt) {
    ASSERT_GT(this->fromPortHistory_dataOut->size(), index);
    const Fw::Buffer& out = this->fromPortHistory_dataOut->at(static_cast<U32>(index)).data;
    ASSERT_TRUE(out.isValid());
    ASSERT_EQ(out.getSize(), pkt.size());
    ASSERT_EQ(::memcmp(out.getData(), pkt.data(), pkt.size()), 0);
    // Delivered buffers come from the dedicated pool, never from the frame storage
    ASSERT_NE(out.getData(), this->m_frame.data());
}

void TcMapReassemblerTester ::assertError(FrameError::T err) {
    ASSERT_from_errorNotify_SIZE(1);
    ASSERT_from_errorNotify(0, FrameError(err));
}

void TcMapReassemblerTester ::assertCounters(U32 reassembled, U32 dropped, U32 abandoned) {
    ASSERT_EQ(this->component.m_packetsReassembled, reassembled);
    ASSERT_EQ(this->component.m_segmentsDropped, dropped);
    ASSERT_EQ(this->component.m_packetsAbandoned, abandoned);
}

void TcMapReassemblerTester ::holdPoolBuffer() {
    Fw::Buffer buffer = this->m_pool.get_bufferGetCallee_InputPort(0)->invoke(MAX_PACKET_SIZE);
    ASSERT_TRUE(buffer.isValid());
    this->m_poolAllocated++;
    if (this->m_poolAllocated > this->m_poolHighWater) {
        this->m_poolHighWater = this->m_poolAllocated;
    }
    this->m_held.push_back(buffer);
}

void TcMapReassemblerTester ::releaseHeldBuffers() {
    for (Fw::Buffer& buffer : this->m_held) {
        this->m_pool.get_bufferSendIn_InputPort(0)->invoke(buffer);
        this->m_poolAllocated--;
    }
    this->m_held.clear();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void TcMapReassemblerTester ::testUnsegmented() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 64);

    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    ASSERT_from_allocate_SIZE(1);
    ASSERT_from_allocate(0, MAX_PACKET_SIZE);
    ASSERT_from_errorNotify_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_PacketsReassembled_SIZE(1);
    ASSERT_TLM_PacketsReassembled(0, 1);
    this->assertCounters(1, 0, 0);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 1U);

    // Returning the packet frees the pool
    this->returnDelivered(0);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_EQ(this->m_poolAllocated, 0U);
    this->assertIdle(mapId);
}

void TcMapReassemblerTester ::testFirstLast() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 100);

    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 40);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_allocate_SIZE(1);
    this->assertInProgress(mapId, 40);

    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 40, 60);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    ASSERT_from_errorNotify_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    this->assertCounters(1, 0, 0);
    this->assertIdle(mapId);

    this->returnDelivered(0);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testFirstContLast() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    const FwSizeType counts[] = {1, 3, 10};
    U32 delivered = 0;
    for (FwSizeType n : counts) {
        // n continuing segments of 16 octets between a 20-octet FIRST and a 9-octet LAST
        const FwSizeType total = 20 + n * 16 + 9;
        std::vector<U8> pkt;
        TcMapReassemblerTester::buildPacket(pkt, total);

        this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 20);
        this->assertInProgress(mapId, 20);
        FwSizeType offset = 20;
        for (FwSizeType i = 0; i < n; i++) {
            // sendSegment asserts the synchronous return of every frame before the next dataIn
            this->sendPortion(mapId, TcSequenceFlags::CONTINUING, pkt, offset, 16);
            offset += 16;
            ASSERT_from_dataOut_SIZE(0);
            ASSERT_from_allocate_SIZE(0);
            this->assertInProgress(mapId, offset);
        }
        this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, offset, 9);
        ASSERT_from_dataOut_SIZE(1);
        this->assertDelivered(0, pkt);
        ASSERT_EVENTS_SIZE(0);
        delivered++;
        this->assertCounters(delivered, 0, 0);
        this->assertIdle(mapId);
        this->returnDelivered(0);
        ASSERT_EQ(this->m_poolAllocated, 0U);
    }
}

void TcMapReassemblerTester ::testShAbsent() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);

    // IDLE: no MAP lookup, no allocation
    this->sendSegment(mapId, TcSequenceFlags::UNSEGMENTED, pkt.data(), pkt.size(), false);
    ASSERT_EVENTS_SegmentHeaderAbsent_SIZE(1);
    this->assertError(FrameError::TC_SEGMENT_HEADER_ABSENT);
    ASSERT_TLM_SegmentsDropped(0, 1);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    this->assertIdle(mapId);

    // IN_PROGRESS: state untouched even though the octet would decode as FIRST on this MAP
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 10);
    this->assertInProgress(mapId, 10);
    this->sendSegment(mapId, TcSequenceFlags::FIRST, pkt.data(), pkt.size(), false);
    ASSERT_EVENTS_SegmentHeaderAbsent_SIZE(1);
    this->assertError(FrameError::TC_SEGMENT_HEADER_ABSENT);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_deallocate_SIZE(0);
    this->assertCounters(0, 2, 0);
    this->assertInProgress(mapId, 10);
}

void TcMapReassemblerTester ::testInvalidMapId() {
    const U8 badMap = this->shadow_unconfiguredMapId();
    if (badMap > 63) {
        GTEST_SKIP() << "every MAP ID is configured (MapChannelCount == 64)";
    }
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);

    this->sendPortion(badMap, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_EVENTS_InvalidMapId_SIZE(1);
    ASSERT_EVENTS_InvalidMapId(0, badMap);
    this->assertError(FrameError::TC_INVALID_MAP_ID);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        this->assertIdle(TcMapReassemblerTester::testMapId(i));
    }
}

void TcMapReassemblerTester ::testOrphanContinuing() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);

    this->sendPortion(mapId, TcSequenceFlags::CONTINUING, pkt, 0, pkt.size());
    ASSERT_EVENTS_UnexpectedSegment_SIZE(1);
    ASSERT_EVENTS_UnexpectedSegment(0, mapId, TcSequenceFlags::CONTINUING);
    this->assertError(FrameError::TC_SEGMENT_ORPHAN);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    this->assertIdle(mapId);
}

void TcMapReassemblerTester ::testOrphanLast() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);

    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 0, pkt.size());
    ASSERT_EVENTS_UnexpectedSegment_SIZE(1);
    ASSERT_EVENTS_UnexpectedSegment(0, mapId, TcSequenceFlags::LAST);
    this->assertError(FrameError::TC_SEGMENT_ORPHAN);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    this->assertIdle(mapId);
}

void TcMapReassemblerTester ::testEmptySegment() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    const TcSequenceFlags::T flags[] = {TcSequenceFlags::FIRST, TcSequenceFlags::CONTINUING, TcSequenceFlags::LAST,
                                        TcSequenceFlags::UNSEGMENTED};
    U32 dropped = 0;

    // IDLE
    for (TcSequenceFlags::T f : flags) {
        this->sendSegment(mapId, f, nullptr, 0);
        ASSERT_EVENTS_EmptySegment_SIZE(1);
        ASSERT_EVENTS_EmptySegment(0, mapId, f);
        this->assertError(FrameError::TC_SEGMENT_EMPTY);
        ASSERT_from_allocate_SIZE(0);
        dropped++;
        this->assertCounters(0, dropped, 0);
        this->assertIdle(mapId);
    }

    // IN_PROGRESS stays IN_PROGRESS with the same received count
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 12);
    this->assertInProgress(mapId, 12);
    for (TcSequenceFlags::T f : flags) {
        this->sendSegment(mapId, f, nullptr, 0);
        ASSERT_EVENTS_EmptySegment_SIZE(1);
        ASSERT_EVENTS_EmptySegment(0, mapId, f);
        this->assertError(FrameError::TC_SEGMENT_EMPTY);
        ASSERT_from_allocate_SIZE(0);
        ASSERT_from_deallocate_SIZE(0);
        dropped++;
        this->assertCounters(0, dropped, 0);
        this->assertInProgress(mapId, 12);
    }
    // The partial is still intact
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 12, 20);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    this->returnDelivered(0);
}

void TcMapReassemblerTester ::testDuplicateFirst() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> first;
    std::vector<U8> second;
    TcMapReassemblerTester::buildPacket(first, 80);
    TcMapReassemblerTester::buildPacket(second, 50);

    this->sendPortion(mapId, TcSequenceFlags::FIRST, first, 0, 30);
    this->assertInProgress(mapId, 30);
    ASSERT_EQ(this->m_poolAllocated, 1U);

    this->sendPortion(mapId, TcSequenceFlags::FIRST, second, 0, 20);
    ASSERT_EVENTS_PacketAbandoned_SIZE(1);
    ASSERT_EVENTS_PacketAbandoned(0, mapId, 30, TcSequenceFlags::FIRST);
    this->assertError(FrameError::TC_SEGMENT_UNEXPECTED_FIRST);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_allocate_SIZE(1);
    ASSERT_TLM_PacketsAbandoned(0, 1);
    this->assertCounters(0, 0, 1);
    this->assertInProgress(mapId, 20);
    // Never more than one in-progress buffer on this MAP
    ASSERT_EQ(this->m_poolAllocated, 1U);
    ASSERT_EQ(this->m_poolHighWater, 1U);

    this->sendPortion(mapId, TcSequenceFlags::LAST, second, 20, 30);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, second);
    this->assertCounters(1, 0, 1);
    this->assertIdle(mapId);
    this->returnDelivered(0);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testUnsegmentedWhileInProgress() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> first;
    std::vector<U8> whole;
    TcMapReassemblerTester::buildPacket(first, 80);
    TcMapReassemblerTester::buildPacket(whole, 24);

    this->sendPortion(mapId, TcSequenceFlags::FIRST, first, 0, 45);
    this->assertInProgress(mapId, 45);

    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, whole, 0, whole.size());
    ASSERT_EVENTS_PacketAbandoned_SIZE(1);
    ASSERT_EVENTS_PacketAbandoned(0, mapId, 45, TcSequenceFlags::UNSEGMENTED);
    this->assertError(FrameError::TC_SEGMENT_UNEXPECTED_UNSEGMENTED);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, whole);
    this->assertCounters(1, 0, 1);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolHighWater, 1U);
    this->returnDelivered(0);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testOverflowSegment() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    // Header stays within bounds so that only the portion size trips the check
    TcMapReassemblerTester::buildPacket(pkt, MAX_PACKET_SIZE + 1, MAX_PACKET_SIZE);

    for (TcSequenceFlags::T f : {TcSequenceFlags::FIRST, TcSequenceFlags::UNSEGMENTED}) {
        this->sendPortion(mapId, f, pkt, 0, pkt.size());
        ASSERT_EVENTS_PacketTooLarge_SIZE(1);
        ASSERT_EVENTS_PacketTooLarge(0, mapId, static_cast<U32>(MAX_PACKET_SIZE + 1),
                                     static_cast<U32>(MAX_PACKET_SIZE));
        this->assertError(FrameError::TC_SEGMENT_OVERFLOW);
        ASSERT_from_allocate_SIZE(0);
        ASSERT_from_dataOut_SIZE(0);
        this->assertIdle(mapId);
    }
    this->assertCounters(0, 2, 0);
}

void TcMapReassemblerTester ::testOverflowAccumulated() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, MAX_PACKET_SIZE + 1, MAX_PACKET_SIZE);

    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, MAX_PACKET_SIZE - 1);
    this->assertInProgress(mapId, MAX_PACKET_SIZE - 1);

    this->sendPortion(mapId, TcSequenceFlags::CONTINUING, pkt, MAX_PACKET_SIZE - 1, 2);
    ASSERT_EVENTS_PacketTooLarge_SIZE(1);
    ASSERT_EVENTS_PacketTooLarge(0, mapId, static_cast<U32>(MAX_PACKET_SIZE + 1), static_cast<U32>(MAX_PACKET_SIZE));
    this->assertError(FrameError::TC_SEGMENT_OVERFLOW);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_TLM_PacketsAbandoned(0, 1);
    this->assertCounters(0, 0, 1);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);

    // Same on LAST
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, MAX_PACKET_SIZE - 1);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, MAX_PACKET_SIZE - 1, 2);
    ASSERT_EVENTS_PacketTooLarge_SIZE(1);
    this->assertError(FrameError::TC_SEGMENT_OVERFLOW);
    ASSERT_from_deallocate_SIZE(1);
    this->assertCounters(0, 0, 2);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testFirstDeclaredTooLarge() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    if (not DECLARED_OVERFLOW_REACHABLE) {
        // MaxPacketSize is the CCSDS maximum: no header can declare more, so a FIRST declaring
        // exactly SpacePacketMaxSize must be accepted rather than rejected as too large
        TcMapReassemblerTester::buildPacket(pkt, 6, SP_MAX_SIZE);
        this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, pkt.size());
        ASSERT_EVENTS_PacketTooLarge_SIZE(0);
        ASSERT_from_errorNotify_SIZE(0);
        ASSERT_from_allocate_SIZE(1);
        this->assertCounters(0, 0, 0);
        this->assertInProgress(mapId, 6);
        return;
    }
    TcMapReassemblerTester::buildPacket(pkt, 6, MAX_PACKET_SIZE + 1);

    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, pkt.size());
    ASSERT_EVENTS_PacketTooLarge_SIZE(1);
    ASSERT_EVENTS_PacketTooLarge(0, mapId, static_cast<U32>(MAX_PACKET_SIZE + 1), static_cast<U32>(MAX_PACKET_SIZE));
    this->assertError(FrameError::TC_SEGMENT_OVERFLOW);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    this->assertIdle(mapId);

    // A 5-octet FIRST carries no complete header: accepted, bound checked later
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 5);
    ASSERT_from_allocate_SIZE(1);
    this->assertInProgress(mapId, 5);
}

void TcMapReassemblerTester ::testUnsegmentedDeclaredTooLarge() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    if (not DECLARED_OVERFLOW_REACHABLE) {
        // MaxPacketSize is the CCSDS maximum: the largest declarable packet is exactly accepted
        TcMapReassemblerTester::buildPacket(pkt, SP_MAX_SIZE);
        this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
        ASSERT_EVENTS_PacketTooLarge_SIZE(0);
        ASSERT_from_errorNotify_SIZE(0);
        ASSERT_from_dataOut_SIZE(1);
        this->assertDelivered(0, pkt);
        this->assertCounters(1, 0, 0);
        this->assertIdle(mapId);
        return;
    }
    TcMapReassemblerTester::buildPacket(pkt, 40, MAX_PACKET_SIZE + 1);

    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_EVENTS_PacketTooLarge_SIZE(1);
    ASSERT_EVENTS_PacketTooLarge(0, mapId, static_cast<U32>(MAX_PACKET_SIZE + 1), static_cast<U32>(MAX_PACKET_SIZE));
    this->assertError(FrameError::TC_SEGMENT_OVERFLOW);
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    this->assertIdle(mapId);
}

void TcMapReassemblerTester ::testAllocFailInvalid() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 40);

    // Exhaust the real pool
    for (FwSizeType i = 0; i < POOL_BUFFER_COUNT; i++) {
        this->holdPoolBuffer();
    }
    ASSERT_EQ(this->m_poolAllocated, POOL_BUFFER_COUNT);

    for (TcSequenceFlags::T f : {TcSequenceFlags::FIRST, TcSequenceFlags::UNSEGMENTED}) {
        this->sendPortion(mapId, f, pkt, 0, pkt.size());
        ASSERT_from_allocate_SIZE(1);
        ASSERT_EVENTS_AllocationFailed_SIZE(1);
        ASSERT_EVENTS_AllocationFailed(0, mapId, static_cast<U32>(MAX_PACKET_SIZE));
        this->assertError(FrameError::TC_SEGMENT_ALLOC_FAILED);
        ASSERT_from_deallocate_SIZE(0);
        ASSERT_from_dataOut_SIZE(0);
        this->assertIdle(mapId);
    }
    this->assertCounters(0, 2, 0);

    // Recovery once the pool has room again
    this->releaseHeldBuffers();
    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    this->assertCounters(1, 2, 0);
    this->returnDelivered(0);
}

void TcMapReassemblerTester ::testAllocFailShort() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 40);

    this->m_allocMode = ALLOC_SHORT;
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, pkt.size());
    ASSERT_from_allocate_SIZE(1);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    ASSERT_EVENTS_AllocationFailed(0, mapId, static_cast<U32>(MAX_PACKET_SIZE));
    this->assertError(FrameError::TC_SEGMENT_ALLOC_FAILED);
    // The short buffer went back exactly once
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getData(), this->m_short.data());
    ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getSize(), MAX_PACKET_SIZE - 1);
    ASSERT_EQ(this->m_shortDeallocated, 1U);
    ASSERT_EQ(this->m_shortOutstanding, 0U);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 1, 0);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);

    // Stub returning a plain invalid buffer takes the same path without a deallocate
    this->m_allocMode = ALLOC_INVALID;
    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_from_allocate_SIZE(1);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    this->assertError(FrameError::TC_SEGMENT_ALLOC_FAILED);
    ASSERT_from_deallocate_SIZE(0);
    this->assertCounters(0, 2, 0);
    this->assertIdle(mapId);

    this->m_allocMode = ALLOC_POOL;
}

void TcMapReassemblerTester ::testLengthMismatchLast() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;

    // Declared larger than accumulated (a segment was lost)
    TcMapReassemblerTester::buildPacket(pkt, 60, 61);
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 30);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 30, 30);
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 60, 61);
    this->assertError(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 0, 1);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);

    // Declared smaller than accumulated (two packets blocked into one FDU)
    TcMapReassemblerTester::buildPacket(pkt, 60, 59);
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 30);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 30, 30);
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 60, 59);
    this->assertError(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 0, 2);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testLengthMismatchUnsegmented() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;

    TcMapReassemblerTester::buildPacket(pkt, 60, 61);
    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 60, 61);
    this->assertError(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
    ASSERT_from_allocate_SIZE(1);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 0, 1);
    this->assertIdle(mapId);

    TcMapReassemblerTester::buildPacket(pkt, 60, 7);
    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 60, 7);
    this->assertError(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
    ASSERT_from_deallocate_SIZE(1);
    this->assertCounters(0, 0, 2);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testTooShortForHeader() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 5);

    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 5, 0);
    this->assertError(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
    ASSERT_from_allocate_SIZE(1);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 0, 1);
    this->assertIdle(mapId);

    // Same through FIRST + LAST totalling 5 octets
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 2);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 2, 3);
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 5, 0);
    this->assertCounters(0, 0, 2);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testMultiMap() {
    if (MAP_CHANNEL_COUNT < 2) {
        GTEST_SKIP() << "requires a TcMapCfg override with MapChannelCount >= 2 (see REPORT.md)";
    }
    const U8 mapA = TcMapReassemblerTester::testMapId(0);
    const U8 mapB = TcMapReassemblerTester::testMapId(1);
    ASSERT_EQ(mapA, 0);
    ASSERT_EQ(mapB, 5);
    std::vector<U8> pktA;
    std::vector<U8> pktB;
    TcMapReassemblerTester::buildPacket(pktA, 90);
    TcMapReassemblerTester::buildPacket(pktB, 70);

    this->sendPortion(mapA, TcSequenceFlags::FIRST, pktA, 0, 50);
    this->assertInProgress(mapA, 50);
    this->assertIdle(mapB);

    this->sendPortion(mapB, TcSequenceFlags::FIRST, pktB, 0, 30);
    this->assertInProgress(mapA, 50);
    this->assertInProgress(mapB, 30);
    ASSERT_EQ(this->m_poolAllocated, 2U);

    this->sendPortion(mapB, TcSequenceFlags::LAST, pktB, 30, 40);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pktB);
    this->assertIdle(mapB);
    this->assertInProgress(mapA, 50);
    this->returnDelivered(0);

    this->sendPortion(mapA, TcSequenceFlags::LAST, pktA, 50, 40);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pktA);
    this->assertIdle(mapA);
    this->assertIdle(mapB);
    this->returnDelivered(0);
    ASSERT_EVENTS_SIZE(0);
    this->assertCounters(2, 0, 0);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testInFlightBound() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);
    std::vector<Fw::Buffer> delivered;
    std::vector<ComCfg::FrameContext> contexts;

    // No partial is in progress, so every pool buffer can be consumed by deliveries
    for (FwSizeType i = 0; i < POOL_BUFFER_COUNT; i++) {
        this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
        ASSERT_from_dataOut_SIZE(1);
        this->assertDelivered(0, pkt);
        delivered.push_back(this->fromPortHistory_dataOut->at(0).data);
        contexts.push_back(this->fromPortHistory_dataOut->at(0).context);
    }
    ASSERT_EQ(this->m_poolAllocated, POOL_BUFFER_COUNT);
    ASSERT_EQ(this->m_poolHighWater, POOL_BUFFER_COUNT);
    this->assertCounters(static_cast<U32>(POOL_BUFFER_COUNT), 0, 0);

    // One more: deterministic AllocationFailed, no state change
    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_from_allocate_SIZE(1);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    ASSERT_EVENTS_AllocationFailed(0, mapId, static_cast<U32>(MAX_PACKET_SIZE));
    this->assertError(FrameError::TC_SEGMENT_ALLOC_FAILED);
    ASSERT_TLM_SegmentsDropped(0, 1);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(static_cast<U32>(POOL_BUFFER_COUNT), 1, 0);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolHighWater, POOL_BUFFER_COUNT);

    // A FIRST is refused as well
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 10);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    this->assertError(FrameError::TC_SEGMENT_ALLOC_FAILED);
    this->assertIdle(mapId);
    this->assertCounters(static_cast<U32>(POOL_BUFFER_COUNT), 2, 0);

    // Returning one packet lets the next one through
    this->clearHistory();
    this->invoke_to_dataReturnIn(0, delivered[0], contexts[0]);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_EQ(this->m_poolAllocated, POOL_BUFFER_COUNT - 1);
    this->sendPortion(mapId, TcSequenceFlags::UNSEGMENTED, pkt, 0, pkt.size());
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    delivered[0] = this->fromPortHistory_dataOut->at(0).data;
    contexts[0] = this->fromPortHistory_dataOut->at(0).context;
    this->assertCounters(static_cast<U32>(POOL_BUFFER_COUNT) + 1, 2, 0);
    ASSERT_EQ(this->m_poolHighWater, POOL_BUFFER_COUNT);

    for (FwSizeType i = 0; i < delivered.size(); i++) {
        this->invoke_to_dataReturnIn(0, delivered[i], contexts[i]);
    }
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testReturnDeallocates() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);

    // A partial in progress must not be disturbed by a return on another buffer
    std::vector<U8> partial;
    TcMapReassemblerTester::buildPacket(partial, 64);
    this->sendPortion(mapId, TcSequenceFlags::FIRST, partial, 0, 20);
    this->assertInProgress(mapId, 20);

    // Take a pool buffer directly and hand it to dataReturnIn as downstream would
    Fw::Buffer buffer = this->m_pool.get_bufferGetCallee_InputPort(0)->invoke(MAX_PACKET_SIZE);
    ASSERT_TRUE(buffer.isValid());
    this->m_poolAllocated++;
    buffer.setSize(static_cast<Fw::Buffer::SizeType>(pkt.size()));
    const ComCfg::FrameContext context = TcMapReassemblerTester::makeContext(TcSequenceFlags::UNSEGMENTED, mapId);

    this->clearHistory();
    this->invoke_to_dataReturnIn(0, buffer, context);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getData(), buffer.getData());
    ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getSize(), buffer.getSize());
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_errorNotify_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_SIZE(0);
    this->assertCounters(0, 0, 0);
    this->assertInProgress(mapId, 20);
    ASSERT_EQ(this->m_poolAllocated, 1U);
}

void TcMapReassemblerTester ::testMacFailureMidPacketModel() {
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 90);

    // FIRST arrives authenticated; the CONTINUING fails its MAC upstream and is never seen here
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 30);
    this->assertInProgress(mapId, 30);
    ASSERT_EQ(this->m_poolAllocated, 1U);

    // The held prefix survives until the next event on the MAP: the LAST reveals the gap
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 60, 30);
    ASSERT_EVENTS_LengthMismatch_SIZE(1);
    ASSERT_EVENTS_LengthMismatch(0, mapId, 60, 90);
    this->assertError(FrameError::TC_SEGMENT_LENGTH_MISMATCH);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    this->assertCounters(0, 0, 1);
    this->assertIdle(mapId);
    ASSERT_EQ(this->m_poolAllocated, 0U);

    // Retransmission of the whole sequence delivers
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 30);
    this->sendPortion(mapId, TcSequenceFlags::CONTINUING, pkt, 30, 30);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 60, 30);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    this->assertCounters(1, 0, 1);
    this->returnDelivered(0);

    // Variant: the loss is followed by a new FIRST instead of a LAST (P4 releases the held buffer)
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 30);
    ASSERT_EQ(this->m_poolAllocated, 1U);
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 30);
    ASSERT_EVENTS_PacketAbandoned_SIZE(1);
    ASSERT_EVENTS_PacketAbandoned(0, mapId, 30, TcSequenceFlags::FIRST);
    this->assertError(FrameError::TC_SEGMENT_UNEXPECTED_FIRST);
    ASSERT_EQ(this->m_poolAllocated, 1U);
    this->sendPortion(mapId, TcSequenceFlags::CONTINUING, pkt, 30, 30);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 60, 30);
    ASSERT_from_dataOut_SIZE(1);
    this->assertDelivered(0, pkt);
    this->assertCounters(2, 0, 2);
    this->returnDelivered(0);
    ASSERT_EQ(this->m_poolAllocated, 0U);
}

void TcMapReassemblerTester ::testConfigureAsserts() {
    const U8 valid[] = {0};
    const U8 outOfRange[] = {64};
    const U8 tooMany[MAP_CHANNEL_COUNT + 1] = {0};

    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(nullptr, 1), "TcMapReassembler.cpp");
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(valid, 0), "TcMapReassembler.cpp");
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(tooMany, MAP_CHANNEL_COUNT + 1), "TcMapReassembler.cpp");
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(outOfRange, 1), "TcMapReassembler.cpp");
    if (MAP_CHANNEL_COUNT >= 2) {
        const U8 duplicate[] = {3, 3};
        ASSERT_DEATH_IF_SUPPORTED(this->component.configure(duplicate, 2), "TcMapReassembler.cpp");
    }

    // A valid table (re)configures without asserting
    U8 table[MAP_CHANNEL_COUNT];
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        table[i] = static_cast<U8>(63 - i);
    }
    this->component.configure(table, MAP_CHANNEL_COUNT);
    ASSERT_EQ(this->component.m_mapCount, MAP_CHANNEL_COUNT);
    ASSERT_NE(this->component.findMap(63), nullptr);
    this->assertIdle(63);
    if (MAP_CHANNEL_COUNT < 64) {
        ASSERT_EQ(this->component.findMap(0), nullptr);
    }
}

void TcMapReassemblerTester ::testSerializedSize() {
    // MF9: the two Segment Header fields (bool + U8) grew the context by 2 octets in PR 1
    static constexpr FwSizeType BASELINE = sizeof(FwIndexType)              // comQueueIndex
                                           + ComCfg::Apid::SERIALIZED_SIZE  // apid
                                           + sizeof(U8)                     // hasSecHdr
                                           + sizeof(U8)                     // sequenceFlags
                                           + sizeof(U16)                    // sequenceCount
                                           + sizeof(U8)                     // vcId
                                           + ComCfg::Pvn::SERIALIZED_SIZE   // pvn
                                           + sizeof(U8)                     // sendNow
                                           + sizeof(U16)                    // saIndex
                                           + sizeof(U16);                   // firstHeaderPointer
    ASSERT_EQ(ComCfg::FrameContext::SERIALIZED_SIZE, BASELINE + 2);

    const ComCfg::FrameContext defaults;
    ASSERT_FALSE(defaults.get_tcSegmentHeaderPresent());
    ASSERT_EQ(defaults.get_tcSegmentHeader(), 0);

    // The context delivered with a packet is the context of the completing segment, SH fields included
    const U8 mapId = TcMapReassemblerTester::testMapId(0);
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, 32);
    this->sendPortion(mapId, TcSequenceFlags::FIRST, pkt, 0, 16);
    this->sendPortion(mapId, TcSequenceFlags::LAST, pkt, 16, 16);
    ASSERT_from_dataOut_SIZE(1);
    const ComCfg::FrameContext& out = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(out, TcMapReassemblerTester::makeContext(TcSequenceFlags::LAST, mapId));
    ASSERT_TRUE(out.get_tcSegmentHeaderPresent());
    ASSERT_EQ(out.get_tcSegmentHeader(), Utils::TcSegmentHeader::encode(TcSequenceFlags::LAST, mapId));
    ASSERT_EQ(out.get_vcId(), TEST_VC_ID);
    this->returnDelivered(0);
}

}  // namespace Ccsds

}  // namespace Svc
