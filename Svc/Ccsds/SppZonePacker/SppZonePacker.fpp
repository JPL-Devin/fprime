module Svc {
module Ccsds {
    @ SppZonePacker packs CCSDS Space Packets into fixed-size data zones, allowing
    @ packets to span consecutive zones of a virtual channel. It sits between
    @ SpacePacketFramer and a fixed-frame framer (TmFramer, AosFramer, or a
    @ fixed-length USLP framer), optionally with CcsdsSdlsFramer in between.
    @ Each emitted zone carries a canonical First Header Pointer in
    @ FrameContext.firstHeaderPointer, which the downstream framer maps to its
    @ protocol wire encoding. Zones are assembled in place inside frame-sized
    @ member buffers with configurable headroom (frame/security headers) and
    @ trailer reserve (security trailer/FECF), enabling zero-copy framing
    @ downstream (FrameContext.zeroCopyFrame is set on emitted zones).
    passive component SppZonePacker {

        import Framer

        @ Rate-group driven flush: emits a partially-filled zone (idle-filled)
        @ when data has been pending since the previous invocation
        sync input port run: Svc.Sched

        @ A partially-filled zone was flushed with idle fill
        event ZoneFlushed(idleBytes: U16) \
            severity diagnostic \
            format "Flushed zone with {} idle fill bytes"

        @ Number of zones emitted downstream
        telemetry ZonesSent: U32 update on change

        @ Number of Space Packets packed into zones
        telemetry PacketsPacked: U32 update on change

        @ Number of idle fill bytes emitted
        telemetry IdleBytesSent: U32 update on change

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
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
