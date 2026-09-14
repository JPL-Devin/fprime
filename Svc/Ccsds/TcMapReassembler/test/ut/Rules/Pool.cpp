// ======================================================================
// \title  Pool.cpp
// \author thomas-bc-autobot
// \brief  Pool-ownership rules for TcMapReassembler (DESIGN §8.4)
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/TcMapReassembler/test/ut/TcMapReassemblerTester.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// ReturnPacket: downstream returns a delivered packet; only a deallocate may follow
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Pool__ReturnPacket__precondition() const {
    return not this->shadow_delivered.empty();
}

void TcMapReassemblerTester ::Pool__ReturnPacket__action() {
    ASSERT_FALSE(this->shadow_delivered.empty());
    const FwSizeType index =
        static_cast<FwSizeType>(STest::Random::lowerUpper(0, static_cast<U32>(this->shadow_delivered.size() - 1)));
    Delivered d = this->shadow_delivered[index];
    this->shadow_delivered.erase(this->shadow_delivered.begin() + static_cast<std::ptrdiff_t>(index));

    const U32 priorReturns = this->m_frameReturns;
    this->clearHistory();
    this->invoke_to_dataReturnIn(0, d.buffer, TcMapReassemblerTester::makeContext(TcSequenceFlags::LAST, 0));
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getData(), d.buffer.getData());
    ASSERT_from_allocate_SIZE(0);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_errorNotify_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_SIZE(0);
    ASSERT_EQ(this->m_frameReturns, priorReturns);
}

// ----------------------------------------------------------------------
// ExhaustPool: take every free pool buffer, drive a FIRST/UNSEGMENTED into the
// empty pool, then give the buffers back
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Pool__ExhaustPool__precondition() const {
    return true;
}

void TcMapReassemblerTester ::Pool__ExhaustPool__action() {
    while (not this->poolFull()) {
        this->holdPoolBuffer();
    }
    ASSERT_EQ(this->m_poolAllocated, POOL_BUFFER_COUNT);

    // With the pool empty a FIRST/UNSEGMENTED on an IDLE MAP is refused (AllocationFailed) and a
    // FIRST/UNSEGMENTED on an IN_PROGRESS MAP first frees the partial, so the allocation succeeds.
    MapShadow& map = this->shadow_maps[this->shadow_randomMapIndex()];
    std::vector<U8> pkt;
    TcMapReassemblerTester::buildPacket(pkt, static_cast<FwSizeType>(STest::Random::lowerUpper(
                                                 static_cast<U32>(SP_MIN_SIZE), static_cast<U32>(MAX_PACKET_SIZE))));
    if (STest::Random::lowerUpper(0, 1) == 0) {
        this->ruleSend(map.mapId, TcSequenceFlags::UNSEGMENTED, pkt.data(), pkt.size());
    } else {
        TcMapReassemblerTester::planPacket(map, pkt.size());
        const FwSizeType portion =
            static_cast<FwSizeType>(STest::Random::lowerUpper(1, static_cast<U32>(pkt.size() - 1)));
        this->ruleSend(map.mapId, TcSequenceFlags::FIRST, map.planned.data(), portion);
    }
    ASSERT_LE(this->m_poolAllocated, POOL_BUFFER_COUNT);
    ASSERT_EQ(this->m_poolHighWater, POOL_BUFFER_COUNT);

    this->releaseHeldBuffers();
}

}  // namespace Ccsds

}  // namespace Svc
