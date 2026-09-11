module Svc {
module Ccsds {
    @ Reassembles Space Packets from authenticated TC Frame Data Units that carry a
    @ Segment Header (CCSDS 232.0-B-4 4.1.3.2.2, 4.4.1, 4.4.3). Multi-MAP, no blocking.
    passive component TcMapReassembler {

        import Deframer

        @ Port to notify upstream of a segment/reassembly error
        output port errorNotify: Ccsds.ErrorNotify

        @ Allocation and deallocation of reassembled-packet buffers (dedicated pool)
        import Svc.BufferAllocation

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ A frame reached the reassembler without a Segment Header in its context (topology misconfiguration)
        event SegmentHeaderAbsent() \
            severity warning high \
            format "Frame without Segment Header context dropped; TcDeframer Segment Header mode is off" \
            throttle 5

        @ Segment Header MAP ID not in the configured accepted set
        event InvalidMapId(mapId: U8) \
            severity warning low \
            format "Segment for unconfigured MAP {} dropped" \
            throttle 10

        @ CONTINUING or LAST segment received while the MAP is IDLE
        event UnexpectedSegment(mapId: U8, flags: TcSequenceFlags) \
            severity warning high \
            format "MAP {}: {} segment with no packet in progress dropped" \
            throttle 10

        @ Partial packet discarded because a FIRST or UNSEGMENTED segment arrived while a packet was in progress
        event PacketAbandoned(mapId: U8, bytesReceived: U32, flags: TcSequenceFlags) \
            severity warning high \
            format "MAP {}: abandoned {} accumulated bytes on {} segment" \
            throttle 10

        @ Segment with zero user-data octets
        event EmptySegment(mapId: U8, flags: TcSequenceFlags) \
            severity warning low \
            format "MAP {}: empty {} segment dropped" \
            throttle 10

        @ Segment, accumulation, or declared Space Packet length exceeds TcMapCfg.MaxPacketSize
        event PacketTooLarge(mapId: U8, bytes: U32, max: U32) \
            severity warning high \
            format "MAP {}: packet of {} bytes exceeds maximum {}; packet discarded" \
            throttle 10

        @ Dedicated pool returned an invalid or short buffer
        event AllocationFailed(mapId: U8, requested: U32) \
            severity warning high \
            format "MAP {}: allocation of {} bytes failed; segment dropped" \
            throttle 10

        @ Accumulated length differs from the Space Packet length declared in its primary header
        event LengthMismatch(mapId: U8, reassembled: U32, declared: U32) \
            severity warning high \
            format "MAP {}: reassembled {} bytes but Space Packet declares {}; packet discarded" \
            throttle 10

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Space Packets delivered downstream
        telemetry PacketsReassembled: U32 update on change

        @ Segments dropped without changing MAP state
        telemetry SegmentsDropped: U32 update on change

        @ Partial packets discarded (abandon, overflow, length mismatch)
        telemetry PacketsAbandoned: U32 update on change

        # ----------------------------------------------------------------------
        # Standard AC Ports
        # ----------------------------------------------------------------------
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut
    }
}
}
