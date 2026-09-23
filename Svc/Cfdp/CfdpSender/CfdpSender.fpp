module Svc {

  @ A component for sending files using the CCSDS File Delivery Protocol (CFDP) Class 1
  @ (Unacknowledged mode). Implements the sending entity procedures per CCSDS 727.0-B-5
  @ Section 4.6.3.2.
  active component CfdpSender {

    # ----------------------------------------------------------------------
    # General Ports
    # ----------------------------------------------------------------------

    @ Scheduled rate input port for driving file transmission
    async input port Run: Svc.Sched

    @ Mutexed port to request a file send (similar to FileDownlink SendFile interface)
    guarded input port SendFile: Svc.SendFileRequest

    @ File complete output port (notifies caller when transfer finishes)
    output port FileComplete: Svc.SendFileComplete

    @ Buffer send output port for transmitting PDUs
    output port bufferSendOut: Fw.BufferSend

    @ Buffer return input port for flow control
    async input port bufferReturn: Fw.BufferSend

    @ Buffer allocate port for obtaining transmit buffers
    output port bufferGetOut: Fw.BufferGet

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
