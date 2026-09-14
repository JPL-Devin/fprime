module Svc {
module Ccsds {
    @ Deframer for the TC Space Data Link Protocol (CCSDS Standard)
    passive component TcDeframer {

        import Deframer

        @ Port to notify of a deframing error
        output port errorNotify: Ccsds.ErrorNotify


        @ Invalid packet received that will be dropped
        event InvalidPacket() \
            severity warning low \
            format "Invalid packet received refusing to deframe"

        @ Deframing received an invalid SCID
        event InvalidSpacecraftId(transmitted: U16, configured: U16) \
            severity warning low \ 
            format "Invalid Spacecraft ID Received. Received: {} | Deframer configured with: {}"

        @ Deframing received an invalid frame length
        event InvalidFrameLength(transmitted: U16, actual: FwSizeType) \
            severity warning high \
            format "Not enough data received. Header length specified: {} | Received data length: {}"

        @ Deframing received an invalid VCID
        event InvalidVcId(transmitted: U16, configured: U16) \
            severity activity low \
            format "Invalid Virtual Channel ID Received. Header token specified: {} | Deframer configured with: {}"

        @ Deframing received an invalid checksum
        event InvalidCrc(transmitted: U16, computed: U16) \
            severity warning high \
            format "Invalid checksum received. Trailer specified: {} | Computed on board: {}"

        @ Segment Header mode on: frame carries no octet after the primary header (CCSDS 232.0-B-4 4.1.3.2.2.1)
        event MissingSegmentHeader(frameLength: U16) \
            severity warning high \
            format "TC frame of {} octets has no Segment Header; frame dropped" \
            throttle 10

        @ Segment Header mode on: Type-BC/AC control frame received; control frames carry no Segment Header (CCSDS 232.0-B-4 4.1.3.2.1.4, 4.1.3.3)
        event ControlFrameDropped(flagsAndScId: U16) \
            severity warning low \
            format "TC control frame (flags/SCID 0x{x}) dropped in Segment Header mode" \
            throttle 10

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
}