// ======================================================================
// \title  Segments.cpp
// \author thomas-bc-autobot
// \brief  Segment-injection rules for TcMapReassembler (DESIGN §8.4)
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/TcMapReassembler/test/ut/TcMapReassemblerTester.hpp"

namespace Svc {

namespace Ccsds {

namespace {

//! Random portion length weighted toward small segments, never above bound
FwSizeType randomPortion(FwSizeType bound) {
    if (bound == 0) {
        return 0;
    }
    if (STest::Random::lowerUpper(0, 3) == 0) {
        return static_cast<FwSizeType>(STest::Random::lowerUpper(1, static_cast<U32>(bound)));
    }
    const FwSizeType small = (bound < 64) ? bound : 64;
    return static_cast<FwSizeType>(STest::Random::lowerUpper(1, static_cast<U32>(small)));
}

//! Random total packet length: mostly a few hundred octets, sometimes up to the bound
FwSizeType randomPacketLength() {
    const FwSizeType maxSize = TcMapReassemblerTestState::MAX_PACKET_SIZE;
    if (STest::Random::lowerUpper(0, 7) == 0) {
        return static_cast<FwSizeType>(STest::Random::lowerUpper(
            static_cast<U32>(TcMapReassemblerTestState::SP_MIN_SIZE), static_cast<U32>(maxSize)));
    }
    const FwSizeType cap = (maxSize < 300) ? maxSize : 300;
    return static_cast<FwSizeType>(
        STest::Random::lowerUpper(static_cast<U32>(TcMapReassemblerTestState::SP_MIN_SIZE), static_cast<U32>(cap)));
}

}  // namespace

// ----------------------------------------------------------------------
// SendFirst: FIRST on any configured MAP (duplicate FIRST when IN_PROGRESS)
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendFirst__precondition() const {
    return true;
}

void TcMapReassemblerTester ::Segments__SendFirst__action() {
    MapShadow& map = this->shadow_maps[this->shadow_randomMapIndex()];
    TcMapReassemblerTester::planPacket(map, randomPacketLength());
    // Strictly less than the whole packet so that a LAST is still expected
    const FwSizeType portion = randomPortion(map.planned.size() - 1);
    this->ruleSend(map.mapId, TcSequenceFlags::FIRST, map.planned.data(), portion);
}

// ----------------------------------------------------------------------
// SendContinuing: next portion of the planned packet on an IN_PROGRESS MAP
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendContinuing__precondition() const {
    return this->shadow_inProgressCount() > 0;
}

void TcMapReassemblerTester ::Segments__SendContinuing__action() {
    const FwSizeType index = this->shadow_randomMapIndex(true);
    ASSERT_LT(index, MAP_CHANNEL_COUNT);
    MapShadow& map = this->shadow_maps[index];
    const FwSizeType have = map.received.size();
    if ((map.planned.size() >= have + 2)) {
        // Leave at least one octet for the LAST
        const FwSizeType portion = randomPortion(map.planned.size() - have - 1);
        this->ruleSend(map.mapId, TcSequenceFlags::CONTINUING, map.planned.data() + have, portion);
    } else {
        // Nothing planned remains (segment loss / duplication model): one stray octet
        this->ruleSend(map.mapId, TcSequenceFlags::CONTINUING, this->randomSegment(1), 1);
    }
}

// ----------------------------------------------------------------------
// SendLast: completes the planned packet, or deliberately mis-sizes it
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendLast__precondition() const {
    return this->shadow_inProgressCount() > 0;
}

void TcMapReassemblerTester ::Segments__SendLast__action() {
    const FwSizeType index = this->shadow_randomMapIndex(true);
    ASSERT_LT(index, MAP_CHANNEL_COUNT);
    MapShadow& map = this->shadow_maps[index];
    const FwSizeType have = map.received.size();
    const bool exact = (map.planned.size() > have) && (STest::Random::lowerUpper(0, 4) != 0);
    if (exact) {
        const FwSizeType remaining = map.planned.size() - have;
        this->ruleSend(map.mapId, TcSequenceFlags::LAST, map.planned.data() + have, remaining);
    } else {
        // Wrong size: a lost or duplicated segment upstream
        const FwSizeType room = (have < MAX_PACKET_SIZE) ? (MAX_PACKET_SIZE - have) : 1;
        const FwSizeType portion = randomPortion(room);
        this->ruleSend(map.mapId, TcSequenceFlags::LAST, this->randomSegment(portion), portion);
    }
}

// ----------------------------------------------------------------------
// SendUnsegmented: one-frame packet on any MAP (supersedes a partial)
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendUnsegmented__precondition() const {
    return true;
}

void TcMapReassemblerTester ::Segments__SendUnsegmented__action() {
    MapShadow& map = this->shadow_maps[this->shadow_randomMapIndex()];
    std::vector<U8> pkt;
    switch (STest::Random::lowerUpper(0, 9)) {
        case 0:
            // Shorter than a Space Packet header
            TcMapReassemblerTester::buildPacket(pkt, static_cast<FwSizeType>(STest::Random::lowerUpper(1, 5)), 0);
            break;
        case 1: {
            // Header disagrees with the actual length
            const FwSizeType len = randomPacketLength();
            FwSizeType declared = randomPacketLength();
            if (declared == len) {
                declared = (len == SP_MIN_SIZE) ? len + 1 : len - 1;
            }
            TcMapReassemblerTester::buildPacket(pkt, len, declared);
            break;
        }
        default:
            TcMapReassemblerTester::buildPacket(pkt, randomPacketLength());
            break;
    }
    this->ruleSend(map.mapId, TcSequenceFlags::UNSEGMENTED, pkt.data(), pkt.size());
}

// ----------------------------------------------------------------------
// SendOrphan: CONTINUING/LAST on an IDLE MAP
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendOrphan__precondition() const {
    return this->shadow_inProgressCount() < MAP_CHANNEL_COUNT;
}

void TcMapReassemblerTester ::Segments__SendOrphan__action() {
    const FwSizeType index = this->shadow_randomMapIndex(false);
    ASSERT_LT(index, MAP_CHANNEL_COUNT);
    const TcSequenceFlags::T flags =
        (STest::Random::lowerUpper(0, 1) == 0) ? TcSequenceFlags::CONTINUING : TcSequenceFlags::LAST;
    const FwSizeType portion = randomPortion(MAX_PACKET_SIZE);
    this->ruleSend(this->shadow_maps[index].mapId, flags, this->randomSegment(portion), portion);
}

// ----------------------------------------------------------------------
// SendInvalidMap: any flags on an unconfigured MAP
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendInvalidMap__precondition() const {
    return this->shadow_unconfiguredMapId() <= 63;
}

void TcMapReassemblerTester ::Segments__SendInvalidMap__action() {
    const U8 mapId = this->shadow_unconfiguredMapId();
    ASSERT_LE(mapId, 63);
    const FwSizeType portion = randomPortion(MAX_PACKET_SIZE);
    this->ruleSend(mapId, TcMapReassemblerTester::randomFlags(), this->randomSegment(portion), portion);
}

// ----------------------------------------------------------------------
// SendEmpty: zero User Data octets with any flags
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendEmpty__precondition() const {
    return true;
}

void TcMapReassemblerTester ::Segments__SendEmpty__action() {
    MapShadow& map = this->shadow_maps[this->shadow_randomMapIndex()];
    this->ruleSend(map.mapId, TcMapReassemblerTester::randomFlags(), nullptr, 0);
}

// ----------------------------------------------------------------------
// SendOversize: every overflow path (P6, P8, P9)
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendOversize__precondition() const {
    return true;
}

void TcMapReassemblerTester ::Segments__SendOversize__action() {
    MapShadow& map = this->shadow_maps[this->shadow_randomMapIndex()];
    const bool startFlags = (STest::Random::lowerUpper(0, 1) == 0);
    const TcSequenceFlags::T start = startFlags ? TcSequenceFlags::FIRST : TcSequenceFlags::UNSEGMENTED;
    // P8 needs a declarable length above MaxPacketSize; at the CCSDS maximum it is unreachable
    const U32 choice = STest::Random::lowerUpper(0, 2);
    switch ((choice == 1 && not DECLARED_OVERFLOW_REACHABLE) ? 0 : choice) {
        case 0: {
            // P6: a single portion of MaxPacketSize + 1 octets
            std::vector<U8> pkt;
            TcMapReassemblerTester::buildPacket(pkt, MAX_PACKET_SIZE + 1, MAX_PACKET_SIZE);
            this->ruleSend(map.mapId, start, pkt.data(), pkt.size());
            break;
        }
        case 1: {
            // P8: header declares more than MaxPacketSize
            std::vector<U8> pkt;
            const FwSizeType len = static_cast<FwSizeType>(
                STest::Random::lowerUpper(static_cast<U32>(SP_HEADER_SIZE), static_cast<U32>(MAX_PACKET_SIZE)));
            const FwSizeType declared = static_cast<FwSizeType>(
                STest::Random::lowerUpper(static_cast<U32>(MAX_PACKET_SIZE + 1), static_cast<U32>(SP_MAX_SIZE)));
            TcMapReassemblerTester::buildPacket(pkt, len, declared);
            this->ruleSend(map.mapId, start, pkt.data(), pkt.size());
            break;
        }
        default: {
            // P9: accumulated overflow on the MAP (an orphan if the MAP is IDLE)
            const TcSequenceFlags::T cont =
                (STest::Random::lowerUpper(0, 1) == 0) ? TcSequenceFlags::CONTINUING : TcSequenceFlags::LAST;
            const FwSizeType portion = MAX_PACKET_SIZE - map.received.size() + 1;
            this->ruleSend(map.mapId, cont, this->randomSegment(portion), portion);
            break;
        }
    }
}

// ----------------------------------------------------------------------
// SendShAbsent: context without a Segment Header (MF1)
// ----------------------------------------------------------------------

bool TcMapReassemblerTester ::Segments__SendShAbsent__precondition() const {
    return true;
}

void TcMapReassemblerTester ::Segments__SendShAbsent__action() {
    // Random MAP ID (configured or not) and random flags: none of them may be looked at
    const U8 mapId = static_cast<U8>(STest::Random::lowerUpper(0, 63));
    const FwSizeType portion = randomPortion(MAX_PACKET_SIZE);
    this->ruleSend(mapId, TcMapReassemblerTester::randomFlags(), this->randomSegment(portion), portion, false);
}

}  // namespace Ccsds

}  // namespace Svc
