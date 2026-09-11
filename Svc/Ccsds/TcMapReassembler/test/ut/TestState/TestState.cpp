// ======================================================================
// \title  TestState.cpp
// \author thomas-bc-autobot
// \brief  Shadow state model for TcMapReassembler rule-based testing
// ======================================================================

#include "Svc/Ccsds/TcMapReassembler/test/ut/TestState/TestState.hpp"

#include "STest/Random/Random.hpp"

namespace Svc {

namespace Ccsds {

// C++14: ODR-used static constexpr members need a definition
constexpr FwSizeType TcMapReassemblerTestState::MAP_CHANNEL_COUNT;
constexpr FwSizeType TcMapReassemblerTestState::MAX_PACKET_SIZE;
constexpr FwSizeType TcMapReassemblerTestState::MAX_PACKETS_IN_FLIGHT;
constexpr FwSizeType TcMapReassemblerTestState::POOL_BUFFER_COUNT;
constexpr FwSizeType TcMapReassemblerTestState::SP_HEADER_SIZE;
constexpr FwSizeType TcMapReassemblerTestState::SP_MIN_SIZE;
constexpr FwSizeType TcMapReassemblerTestState::SP_MAX_SIZE;
constexpr bool TcMapReassemblerTestState::DECLARED_OVERFLOW_REACHABLE;

FwSizeType TcMapReassemblerTestState ::shadow_inProgressCount() const {
    FwSizeType count = 0;
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        if (this->shadow_maps[i].inProgress) {
            count++;
        }
    }
    return count;
}

const TcMapReassemblerTestState::MapShadow* TcMapReassemblerTestState ::shadow_findMap(U8 mapId) const {
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        if (this->shadow_maps[i].mapId == mapId) {
            return &this->shadow_maps[i];
        }
    }
    return nullptr;
}

FwSizeType TcMapReassemblerTestState ::shadow_randomMapIndex() const {
    return static_cast<FwSizeType>(STest::Random::lowerUpper(0, static_cast<U32>(MAP_CHANNEL_COUNT - 1)));
}

FwSizeType TcMapReassemblerTestState ::shadow_randomMapIndex(bool inProgress) const {
    FwSizeType candidates = 0;
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        if (this->shadow_maps[i].inProgress == inProgress) {
            candidates++;
        }
    }
    if (candidates == 0) {
        return MAP_CHANNEL_COUNT;
    }
    U32 pick = STest::Random::lowerUpper(0, static_cast<U32>(candidates - 1));
    for (FwSizeType i = 0; i < MAP_CHANNEL_COUNT; i++) {
        if (this->shadow_maps[i].inProgress == inProgress) {
            if (pick == 0) {
                return i;
            }
            pick--;
        }
    }
    return MAP_CHANNEL_COUNT;
}

U8 TcMapReassemblerTestState ::shadow_unconfiguredMapId() const {
    // Start from a random candidate so every unconfigured ID is reachable
    const U8 start = static_cast<U8>(STest::Random::lowerUpper(0, 63));
    for (U8 i = 0; i < 64; i++) {
        const U8 candidate = static_cast<U8>((start + i) % 64);
        if (this->shadow_findMap(candidate) == nullptr) {
            return candidate;
        }
    }
    return 64;
}

U32 TcMapReassemblerTestState ::shadow_terminalOutcomes() const {
    return this->shadow_packetsReassembled + this->shadow_packetsAbandoned + this->shadow_segmentsDropped;
}

}  // namespace Ccsds

}  // namespace Svc
