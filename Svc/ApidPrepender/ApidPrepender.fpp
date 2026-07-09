module Svc {

    @ Prepends the APID (packet type) to a buffer so that it survives transit
    @ over APID-agnostic Fw.BufferSend links, such as the GenericHub pattern.
    @ Pair with an ApidStripper component on the receiving side.
    passive component ApidPrepender {

        @ Port receiving data with its APID
        sync input port dataIn: Fw.BufferWithApid

        @ Port returning ownership of buffers received on dataIn to their sender
        output port dataReturnOut: Fw.BufferSend

        @ Port emitting the APID-prepended buffer
        output port dataOut: Fw.BufferSend

        @ Port receiving back ownership of buffers emitted on dataOut
        sync input port dataOutReturn: Fw.BufferSend

        @ This interface provides ports allocate and deallocate
        import Svc.BufferAllocation

        @ Port for emitting events
        event port Log

        @ Port for emitting text events
        text event port LogText

        @ Port for getting the time
        time get port Time

        @ Failed to allocate a buffer for the APID-prepended data
        event AllocationFailed(
            $size: FwSizeType @< The requested allocation size
        ) severity warning high format "Failed to allocate buffer of size {}, dropping data"

    }

}
