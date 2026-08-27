// ======================================================================
// \title  SppZonePacker.hpp
// \author devin
// \brief  hpp file for SppZonePacker component implementation class
// ======================================================================

#ifndef Svc_Ccsds_SppZonePacker_HPP
#define Svc_Ccsds_SppZonePacker_HPP

#include "Fw/Buffer/Buffer.hpp"
#include "Os/Mutex.hpp"
#include "Svc/Ccsds/SppZonePacker/SppZonePackerComponentAc.hpp"
#include "Svc/Ccsds/Types/SpacePacketHeaderSerializableAc.hpp"
#include "config/FppConstantsAc.hpp"

namespace Svc {

namespace Ccsds {

class SppZonePacker final : public SppZonePackerComponentBase {
    friend class SppZonePackerTester;

    static constexpr U8 SPP_IDLE_DATA_PATTERN = 0x44;
    static constexpr U8 MIN_SPP_LENGTH = SpacePacketHeader::SERIALIZED_SIZE + 1;
    static constexpr FwSizeType POOL_DEPTH = 2;
    static constexpr U8 MAX_CREDIT_OWED = 2;

    enum class BufferOwnershipState {
        NOT_OWNED,  //!< The buffer is currently held downstream
        OWNED,      //!< The buffer is currently owned by the SppZonePacker
    };

    //! A packet held across zone boundaries (or an idle packet striped across zones)
    struct OutstandingPacket {
        Fw::Buffer packet;             //!< Packet buffer to continue packing
        ComCfg::FrameContext context;  //!< Context of the above packet
        FwSizeType offset = 0;         //!< Offset into the packet of the next byte to pack
        bool isIdle = false;           //!< Packet is the member idle packet (no upstream return/credit)
    };

    //! One pool entry: a frame-sized backing store for in-place zone assembly
    struct PoolEntry {
        U8 backer[ComCfg::MaxTransferFrameSize] = {};
        BufferOwnershipState state = BufferOwnershipState::OWNED;
    };

    //! Per virtual channel packing state (sized 1 for now; keyed extension point for multi-VC)
    struct ZoneVc {
        bool configured = false;
        U8 virtualChannelId = 0;

        PoolEntry pool[POOL_DEPTH];
        FwIndexType fillIndex = 0;  //!< Pool entry currently under construction

        U16 payloadOffset = 0;              //!< Bytes packed into the zone under construction
        bool pastFirstFreshPacket = false;  //!< A fresh packet header was placed in this zone
        U16 fhp = 0;                        //!< Offset of the first fresh packet header (valid when past flag set)
        bool realDataInZone = false;        //!< The zone under construction holds non-idle bytes

        OutstandingPacket outstanding;

        //! Backing store for a minimum-size idle packet striped across a zone boundary
        U8 sppIdleBacker[MIN_SPP_LENGTH] = {};

        ComCfg::FrameContext lastContext;  //!< Context of the last packed packet (used for emitted zones)

        bool packetCreditPending = false;  //!< A consumed input packet's SUCCESS credit is owed on zone return
        bool sendPendingFull = false;      //!< The zone under construction is full, waiting for the in-flight
                                           //!< zone to return before being sent
        bool flushPending = false;         //!< A flush was requested while a zone was in flight or uncredited
        U8 creditOwed = 0;                 //!< Downstream SUCCESS statuses to absorb (bounded by MAX_CREDIT_OWED)
    };

    //! Output actions collected under the mutex and emitted after unlock
    struct Emission {
        bool sendZone = false;
        Fw::Buffer zone;
        ComCfg::FrameContext zoneContext;

        bool returnPacket = false;
        Fw::Buffer packet;
        ComCfg::FrameContext packetContext;

        bool sendStatus = false;
        Fw::Success status = Fw::Success::SUCCESS;

        bool flushed = false;
        U16 flushIdleBytes = 0;

        U32 zonesSent = 0;
        U32 packetsPacked = 0;
        U32 idleBytesSent = 0;
    };

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SppZonePacker object
    SppZonePacker(const char* const compName  //!< The component name
    );

    //! Destroy SppZonePacker object
    ~SppZonePacker();

    //! Configure the zone geometry for the (single) virtual channel
    //!
    //! The emitted zone buffer has `headroom` bytes of reserved capacity before the
    //! zone (frame primary header plus optional security header) and `trailerReserve`
    //! bytes after it (optional security trailer plus FECF), so downstream framers can
    //! assemble the transfer frame in place around the zone
    void configure(FwSizeType zoneSize,        //!< Size in bytes of the packet zone
                   FwSizeType headroom,        //!< Reserved bytes before the zone
                   FwSizeType trailerReserve,  //!< Reserved bytes after the zone
                   U8 vcId                     //!< Virtual channel this packer serves
    );

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comStatusIn
    //!
    //! Port receiving the general status from the downstream component
    void comStatusIn_handler(FwIndexType portNum,  //!< The port number
                             Fw::Success& condition) override;

    //! Handler implementation for dataIn
    //!
    //! Port to receive data to frame, in a Fw::Buffer with optional context
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of emitted zone buffers
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,
                              const ComCfg::FrameContext& context) override;

    //! Handler implementation for run
    //!
    //! Rate-group driven flush of partially-filled zones
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context) override;

    // ----------------------------------------------------------------------
    // Helpers (all called with the mutex held unless noted)
    // ----------------------------------------------------------------------

    //! Map a frame context onto the per-VC state (single VC for now)
    ZoneVc& getVc(const ComCfg::FrameContext& context);

    //! Build the zone window buffer over the fill pool entry (offset = headroom, size = zone size)
    Fw::Buffer zoneWindow(ZoneVc& vc);

    //! Snapshot telemetry counters into the emission (written after unlock)
    void snapshotTelemetry(Emission& em);

    //! Pack bytes of a packet into the zone under construction, clamping at the
    //! zone boundary and recording any leftover as the outstanding fragment
    void packBytes(ZoneVc& vc,
                   Fw::Buffer& data,
                   const ComCfg::FrameContext& context,
                   FwSizeType dataOffset,
                   bool isIdle,
                   Emission& em);

    //! Handle zone-full/credit bookkeeping after packing; fills Emission
    void finishPacking(ZoneVc& vc, Emission& em);

    //! Prepare the full zone under construction for sending: builds the zone
    //! buffer/context, swaps the fill buffer, and resets per-zone state
    void prepareZoneSend(ZoneVc& vc, Emission& em);

    //! Fill the remainder of the zone with an idle packet (striping across the
    //! zone boundary when fewer than MIN_SPP_LENGTH bytes remain)
    void fillWithIdle(ZoneVc& vc, Emission& em);

    //! Flush the partially-filled zone if the VC is quiescent; defers otherwise
    void tryFlush(ZoneVc& vc, Emission& em);

    //! True when no zone is in flight and no downstream credit is outstanding
    bool isQuiescent(const ZoneVc& vc) const;

    //! Serialize a Space Packet idle packet of the given total length
    static void serializeIdlePacket(Fw::SerialBufferBase& serializer, U16 length);

    //! True when the buffer points within the given backing store
    static bool bufferBelongs(const Fw::Buffer& buffer, const U8* start, FwSizeType size);

    //! Emit the collected outputs (called WITHOUT the mutex held)
    void emit(const Emission& em);

    // ----------------------------------------------------------------------
    // Members
    // ----------------------------------------------------------------------

    FwSizeType m_zoneSize = 0;
    FwSizeType m_headroom = 0;
    FwSizeType m_trailerReserve = 0;

    ZoneVc m_vcs[1];

    U32 m_zonesSent = 0;
    U32 m_packetsPacked = 0;
    U32 m_idleBytesSent = 0;

    Os::Mutex m_mutex;
};

}  // namespace Ccsds

}  // namespace Svc

#endif
