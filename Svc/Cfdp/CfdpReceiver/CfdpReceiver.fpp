module Svc {

  @ A component for receiving files using the CCSDS File Delivery Protocol (CFDP) Class 1
  @ (Unacknowledged mode). Implements the receiving entity procedures per CCSDS 727.0-B-5
  @ Section 4.6.3.3.
  active component CfdpReceiver {

    # ----------------------------------------------------------------------
    # General Ports
    # ----------------------------------------------------------------------

    @ Buffer input port for receiving CFDP PDUs
    async input port bufferSendIn: Fw.BufferSend

    @ Buffer return output port to deallocate received buffers
    output port bufferSendOut: Fw.BufferSend

    @ Announce a received file for further processing
    output port fileAnnounce: Svc.FileAnnounce

    @ Ping input port for health monitoring
    async input port pingIn: Svc.Ping

    @ Ping output port for health monitoring
    output port pingOut: Svc.Ping

    # ----------------------------------------------------------------------
    # Special Ports
    # ----------------------------------------------------------------------

    @ Time get port
    time get port timeCaller

    @ Command registration port
    command reg port cmdRegOut

    @ Command receive port
    command recv port cmdIn

    @ Command response port
    command resp port cmdResponseOut

    @ Event port
    event port eventOut

    @ Text event port
    text event port textEventOut

    @ Telemetry port
    telemetry port tlmOut

    # ----------------------------------------------------------------------
    # Commands
    # ----------------------------------------------------------------------

    include "Commands.fppi"

    # ----------------------------------------------------------------------
    # Telemetry
    # ----------------------------------------------------------------------

    include "Telemetry.fppi"

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    include "Events.fppi"

  }

}
