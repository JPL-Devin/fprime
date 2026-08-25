module Svc {
module Ccsds {
    @ Deframer for the Unified Space Data Link Protocol (USLP)
    @ Per CCSDS 732.1-B-3 - Unified Space Data Link Protocol
    @ Deframes non-truncated USLP Transfer Frames carrying a variable-length
    @ TFDZ with no segmentation (construction rule 0b111) and a mandatory FECF
    passive component UslpDeframer {

        import Deframer

        @ Port to notify of a deframing error
        output port errorNotify: Ccsds.ErrorNotify

        @ Invalid packet received that will be dropped
        event InvalidPacket() \
            severity warning low \
            format "Invalid packet received refusing to deframe"

        @ Deframing received an invalid Transfer Frame Version Number
        event InvalidFrameVersion(transmitted: U8, expected: U8) \
            severity warning low \
            format "Invalid Transfer Frame Version Number Received. Received: {} | Expected: {}"

        @ Deframing received a truncated frame (End of Frame Primary Header flag set)
        event TruncatedFrameNotSupported() \
            severity warning low \
            format "Truncated USLP frame received (EOFPH flag set), truncated frames are not supported"

        @ Deframing received an invalid SCID
        event InvalidSpacecraftId(transmitted: U16, configured: U16) \
            severity warning low \
            format "Invalid Spacecraft ID Received. Received: {} | Deframer configured with: {}"

        @ Deframing received a frame not addressed to the spacecraft
        event InvalidSourceOrDest(transmitted: U8) \
            severity warning low \
            format "Invalid Source-or-Destination Identifier Received: {} | Uplink frames must carry 1 (spacecraft is destination)"

        @ Deframing received an invalid frame length
        event InvalidFrameLength(transmitted: U16, actual: FwSizeType) \
            severity warning high \
            format "Invalid frame length. Header length field specified: {} | Received data length: {}"

        @ Deframing received an invalid VCID
        event InvalidVcId(transmitted: U8, configured: U8) \
            severity activity low \
            format "Invalid Virtual Channel ID Received. Received: {} | Deframer configured with: {}"

        @ Deframing received an invalid MAP ID
        event InvalidMapId(transmitted: U8, configured: U8) \
            severity activity low \
            format "Invalid MAP ID Received. Received: {} | Deframer configured with: {}"

        @ Deframing received a protocol control command frame
        event ProtocolCommandNotSupported() \
            severity warning low \
            format "USLP protocol control command frame received, protocol control commands are not supported"

        @ Deframing received non-zero spare bits
        event InvalidSpareBits(transmitted: U8) \
            severity warning low \
            format "Invalid spare bits received: {} | Spare bits must be 0"

        @ Deframing received a frame with the OCF flag set
        event OcfNotSupported() \
            severity warning low \
            format "USLP frame received with OCF flag set, Operational Control Field is not supported"

        @ Deframing received an unexpected VCF count length
        event InvalidVcfCountLength(transmitted: U8, configured: U8) \
            severity warning low \
            format "Invalid VCF Count Length Received. Received: {} | Deframer configured with: {}"

        @ Deframing received an invalid checksum
        event InvalidCrc(transmitted: U16, computed: U16) \
            severity warning high \
            format "Invalid checksum received. Trailer specified: {} | Computed on board: {}"

        @ Deframing received an unsupported TFDF construction rule
        event InvalidTfdfRule(transmitted: U8, expected: U8) \
            severity warning low \
            format "Invalid TFDF construction rule received: {} | Expected: {}"

        @ USLP Protocol Identifier of the deframed TFDF
        event UpidReceived(upid: U8) \
            severity diagnostic \
            format "USLP Protocol Identifier received: {}"

        @ Number of frames successfully deframed
        telemetry FramesProcessed: U32 \
            format "{} frames processed"

        @ Frame Error Control Field (FECF) errors
        telemetry CrcErrorCount: U32 \
            format "{} FECF errors"

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
