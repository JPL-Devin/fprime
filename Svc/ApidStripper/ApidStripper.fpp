module Svc {

    @ Strips a leading APID (packet type) from a buffer received over an
    @ APID-agnostic Fw.BufferSend link, such as the GenericHub pattern, and
    @ re-emits it as an explicit port argument. Pair with an ApidPrepender
    @ component on the sending side.
    passive component ApidStripper {

        @ Port receiving APID-prepended buffers
        sync input port dataIn: Fw.BufferSend

        @ Port emitting the stripped buffer with its APID
        output port dataOut: Fw.BufferWithApid

        @ Port receiving back ownership of buffers emitted on dataOut
        sync input port dataReturnIn: Fw.BufferSend

        @ Port returning ownership of buffers received on dataIn to their sender
        output port dataReturnOut: Fw.BufferSend

        @ Port for emitting events
        event port Log

        @ Port for emitting text events
        text event port LogText

        @ Port for getting the time
        time get port Time

        @ Received a buffer too small to contain an APID
        event BufferTooSmall(
            $size: FwSizeType @< The received buffer size
        ) severity warning high format "Received buffer of size {} too small to contain an APID, dropping data"

    }

}
