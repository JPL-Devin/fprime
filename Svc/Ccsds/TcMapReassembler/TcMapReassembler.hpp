// ======================================================================
// \title  TcMapReassembler.hpp
// \author thomas-bc-autobot
// \brief  hpp file for TcMapReassembler component implementation class
// ======================================================================

#ifndef Svc_Ccsds_TcMapReassembler_HPP
#define Svc_Ccsds_TcMapReassembler_HPP

#include "Svc/Ccsds/TcMapReassembler/TcMapReassemblerComponentAc.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/FrameErrorEnumAc.hpp"
#include "Svc/Ccsds/Types/TcSequenceFlagsEnumAc.hpp"
#include "TcMapReassemblerConfig/FppConstantsAc.hpp"

namespace Svc {
namespace Ccsds {

// fpp-to-cpp emits each integer constant as its own anonymous enum. Comparing two such enumerators directly
// trips -Werror=enum-compare, and GCC folds a comparison of two cast enumerators back onto the enum types
// (-Wconversion "outside the range"), so every constant is bound to a named constexpr first.
namespace {
constexpr FwSizeType kMapChannelCount = TcMapCfg::MapChannelCount;
constexpr FwSizeType kMaxPacketSize = TcMapCfg::MaxPacketSize;
constexpr FwSizeType kMaxPacketsInFlight = TcMapCfg::MaxPacketsInFlight;
constexpr FwSizeType kPoolBufferCount = TcMapCfg::PoolBufferCount;
constexpr U64 kPoolBytes = TcMapCfg::PoolBytes;
constexpr FwSizeType kSpacePacketMinSize = TcMapCfg::SpacePacketMinSize;
constexpr FwSizeType kSpacePacketMaxSize = TcMapCfg::SpacePacketMaxSize;
}  // namespace

static_assert(kMapChannelCount >= 1 && kMapChannelCount <= 64,
              "MapChannelCount must be 1..64 (6-bit MAP ID, 232.0-B-4 4.1.3.2.2.3)");
static_assert(kMaxPacketSize >= kSpacePacketMinSize && kMaxPacketSize <= kSpacePacketMaxSize,
              "MaxPacketSize must be 7..65542 (133.0-B-2 4.1.2.2)");
static_assert(kMaxPacketsInFlight >= 1, "MaxPacketsInFlight must be >= 1");
static_assert(kPoolBufferCount == kMapChannelCount + kMaxPacketsInFlight, "PoolBufferCount arithmetic");
static_assert(kPoolBufferCount <= 65535, "Svc::BufferManager::BufferBin::numBuffers is U16");
static_assert(kPoolBytes == static_cast<U64>(kPoolBufferCount) * static_cast<U64>(kMaxPacketSize),
              "PoolBytes arithmetic (MF11)");

//! Reassembles Space Packets from TC Frame Data Units carrying a Segment Header (CCSDS 232.0-B-4 4.4.1,
//! 4.4.3). Sits after the SDLS SUCCESS gate: every octet it sees is authenticated. Copy-always: every
//! incoming frame is returned upstream synchronously from dataIn; delivered packets live in buffers from
//! the dedicated pool reached through allocate/deallocate. A MAP is a channel *within* a Virtual
//! Channel (232.0-B-4 2.1.3), so reassembly state is keyed by the (VCID, MAP ID) pair.
class TcMapReassembler final : public TcMapReassemblerComponentBase {
    friend class TcMapReassemblerTester;

  public:
    // ----------------------------------------------------------------------
    // Types
    // ----------------------------------------------------------------------

    //! Identity of one reassembly channel: a MAP of one Virtual Channel
    struct MapKey {
        U8 vcId;   //!< TC Virtual Channel ID (0..63), matched against FrameContext.vcId
        U8 mapId;  //!< MAP ID (0..63) of the Segment Header
    };

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TcMapReassembler object
    explicit TcMapReassembler(const char* const compName  //!< The component name
    );

    //! Destroy TcMapReassembler object
    ~TcMapReassembler();

    TcMapReassembler(const TcMapReassembler&) = delete;
    TcMapReassembler& operator=(const TcMapReassembler&) = delete;

    //! \brief Configure the accepted (Virtual Channel, MAP ID) pairs (CCSDS 232.0-B-4 Table 5-3 "Valid MAP IDs")
    //!
    //! Call once during topology setup, before any frame is processed. Each pair gets one
    //! reassembly slot; the same MAP ID on two Virtual Channels is two independent channels.
    //! Asserts on a null table, a count outside 1..TcMapCfg::MapChannelCount, a VCID or MAP ID
    //! above 63, or a duplicate pair: these are configuration errors, not ground input.
    void configure(const MapKey* channels,  //!< Accepted (VCID, MAP ID) pairs
                   FwSizeType count         //!< Number of entries in channels
    );

  private:
    // ----------------------------------------------------------------------
    // Types
    // ----------------------------------------------------------------------

    //! Reassembly state of one MAP channel (232.0-B-4 4.1.3.2.2.3)
    struct MapChannel {
        enum State : U8 { IDLE = 0, IN_PROGRESS = 1 };
        U8 vcId = 0;              //!< configured Virtual Channel ID (0..63)
        U8 mapId = 0;             //!< configured MAP ID (0..63)
        State state = IDLE;       //!< IDLE at construction and after every delivery/abandon
        Fw::Buffer buffer;        //!< valid iff IN_PROGRESS; capacity == TcMapCfg::MaxPacketSize
        FwSizeType received = 0;  //!< octets copied so far
        U16 segments = 0;         //!< segments accumulated
    };

    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! One authenticated Frame Data Unit (Segment Header already stripped into the context).
    //! Guarded: the only writer of the MAP state. Returns the frame on dataReturnOut in every path.
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! A delivered packet buffer coming back from downstream: deallocated to the pool. Never
    //! touches the MAP state, so it may run on any thread without locking.
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,
                              const ComCfg::FrameContext& context) override;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Find the reassembly slot for a (VCID, MAP ID) pair; nullptr if not configured (232.0-B-4 4.4.3.3)
    MapChannel* findMap(U8 vcId, U8 mapId);

    //! Start a new packet on an IDLE MAP from a FIRST or UNSEGMENTED segment (P6, P8, P7, P12)
    //! \return true if the segment was copied into a freshly allocated pool buffer
    bool startPacket(MapChannel& ch, Fw::Buffer& data, const ComCfg::FrameContext& context);

    //! Exact-length check and delivery of a completed packet (P10, P11)
    void complete(MapChannel& ch, const ComCfg::FrameContext& context);

    //! Discard the partial packet of a MAP: deallocate, IDLE, PacketsAbandoned++, errorNotify
    void abandon(MapChannel& ch, FrameError::T err);

    //! Drop a segment without changing MAP state: SegmentsDropped++, errorNotify, return the frame
    void reject(FrameError::T err, Fw::Buffer& data, const ComCfg::FrameContext& context);

    //! Emit an errorNotify port message if the port is connected
    void notifyError(FrameError::T err);

    //! Total Space Packet length declared by a primary header (133.0-B-2 4.1.3.5)
    //! \param p at least TcMapCfg::SpacePacketHeaderSize octets
    static FwSizeType declaredLength(const U8* const p);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    MapChannel m_maps[TcMapCfg::MapChannelCount];  //!< one slot per configured (VCID, MAP) pair; fixed array
    FwSizeType m_mapCount = 0;                     //!< entries of m_maps set by configure()
    U32 m_packetsReassembled = 0;                  //!< PacketsReassembled telemetry
    U32 m_segmentsDropped = 0;                     //!< SegmentsDropped telemetry
    U32 m_packetsAbandoned = 0;                    //!< PacketsAbandoned telemetry
};

}  // namespace Ccsds
}  // namespace Svc

#endif
