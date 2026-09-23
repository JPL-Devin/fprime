// ======================================================================
// \title  TestState.hpp
// \author thomas-bc-autobot
// \brief  Shadow state model for TcMapReassembler rule-based testing
//
// The shadow mirrors what the component must hold: per configured MAP the
// in-progress flag and the authenticated octets accumulated so far, the
// packets delivered downstream and not yet returned, and the three counters.
// Rule preconditions read from it; rule actions update it in lockstep with
// the component and the invariants of DESIGN §8.4 compare the two.
// ======================================================================

#ifndef Svc_Ccsds_TcMapReassembler_TestState_HPP
#define Svc_Ccsds_TcMapReassembler_TestState_HPP

#include <vector>

#include "Fw/Buffer/Buffer.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "TcMapReassemblerConfig/FppConstantsAc.hpp"

namespace Svc {

namespace Ccsds {

class TcMapReassemblerTestState {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    static constexpr FwSizeType MAP_CHANNEL_COUNT = static_cast<FwSizeType>(TcMapCfg::MapChannelCount);
    static constexpr FwSizeType MAX_PACKET_SIZE = static_cast<FwSizeType>(TcMapCfg::MaxPacketSize);
    static constexpr FwSizeType MAX_PACKETS_IN_FLIGHT = static_cast<FwSizeType>(TcMapCfg::MaxPacketsInFlight);
    static constexpr FwSizeType POOL_BUFFER_COUNT = static_cast<FwSizeType>(TcMapCfg::PoolBufferCount);
    static constexpr FwSizeType SP_HEADER_SIZE = static_cast<FwSizeType>(TcMapCfg::SpacePacketHeaderSize);
    static constexpr FwSizeType SP_MIN_SIZE = static_cast<FwSizeType>(TcMapCfg::SpacePacketMinSize);
    static constexpr FwSizeType SP_MAX_SIZE = static_cast<FwSizeType>(TcMapCfg::SpacePacketMaxSize);
    //! P8 (declared length above MaxPacketSize) is reachable only when the Packet Data Length field can express it
    static constexpr bool DECLARED_OVERFLOW_REACHABLE = (MAX_PACKET_SIZE < SP_MAX_SIZE);

    // ----------------------------------------------------------------------
    // Types
    // ----------------------------------------------------------------------

    //! Shadow of one MAP channel
    struct MapShadow {
        U8 mapId = 0;              //!< configured MAP ID
        bool inProgress = false;   //!< component must be IN_PROGRESS with a valid buffer
        std::vector<U8> received;  //!< octets the component must hold (size == received)
        std::vector<U8> planned;   //!< the Space Packet the current FIRST..LAST sequence carries
    };

    //! A packet delivered on dataOut and not yet returned on dataReturnIn
    struct Delivered {
        Fw::Buffer buffer;      //!< buffer as delivered
        std::vector<U8> bytes;  //!< content the buffer must carry
    };

  public:
    // ----------------------------------------------------------------------
    // Component "shadow" test state
    // ----------------------------------------------------------------------

    MapShadow shadow_maps[MAP_CHANNEL_COUNT];
    std::vector<Delivered> shadow_delivered;
    U32 shadow_packetsReassembled = 0;
    U32 shadow_segmentsDropped = 0;
    U32 shadow_packetsAbandoned = 0;
    U32 shadow_dataInCalls = 0;

    //! Number of times each event was raised (throttled events are not emitted past their throttle)
    struct EventCounts {
        U32 segmentHeaderAbsent = 0;
        U32 invalidMapId = 0;
        U32 unexpectedSegment = 0;
        U32 packetAbandoned = 0;
        U32 emptySegment = 0;
        U32 packetTooLarge = 0;
        U32 allocationFailed = 0;
        U32 lengthMismatch = 0;
    };
    EventCounts shadow_events;

  public:
    // ----------------------------------------------------------------------
    // Helpers querying the shadow state
    // ----------------------------------------------------------------------

    //! Number of MAPs IN_PROGRESS
    FwSizeType shadow_inProgressCount() const;

    //! Shadow of a configured MAP, nullptr if the MAP ID is not configured
    const MapShadow* shadow_findMap(U8 mapId) const;

    //! Index of a uniformly random configured MAP
    FwSizeType shadow_randomMapIndex() const;

    //! Index of a uniformly random MAP in the given state, MAP_CHANNEL_COUNT if none
    FwSizeType shadow_randomMapIndex(bool inProgress) const;

    //! A MAP ID that is not configured; 64 (invalid) if every MAP ID is configured
    U8 shadow_unconfiguredMapId() const;

    //! Number of terminal outcomes the shadow modelled (invariant 6)
    U32 shadow_terminalOutcomes() const;
};

}  // namespace Ccsds

}  // namespace Svc

#endif
