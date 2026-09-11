module Svc {

  @ A passive component for mediating between the PassiveBufferDriver
  @ interface (as used by Svc.GenericHub) and the Svc.ComDataWithContext
  @ ports of the framing stack (Svc.Framer, Svc.Deframer)
  @
  @ Sample topology:
  @
  @   framer.dataIn        <-- dataOut       ComDataBufferAdapter  bufferIn        <-- client.toBufferDriver
  @   framer.dataReturnOut --> dataReturnIn                        bufferInReturn  --> client.toBufferDriverReturn
  @   deframer.dataOut     --> dataIn                              bufferOut       --> client.fromBufferDriver
  @   deframer.dataReturnIn <- dataReturnOut                       bufferOutReturn <-- client.fromBufferDriverReturn
  @
  @ Together with a framer, a deframer, and a com interface (e.g. Svc.ComStub
  @ over a Drv.ByteStreamDriver), this component functions as a
  @ PassiveBufferDriver for a PassiveBufferDriverClient such as Svc.GenericHub.
  @
  @ The framer and deframer connected to this component must be dedicated
  @ to it: dataOut/dataReturnIn form a single ownership loop with one
  @ framer, and dataIn/dataReturnOut with one deframer.
  passive component ComDataBufferAdapter {

    @ ComDataBufferAdapter is a PassiveBufferDriver
    import Drv.PassiveBufferDriver

    # ----------------------------------------------------------------------
    # Send side (buffer client -> framer)
    # ----------------------------------------------------------------------

    @ Port for sending buffers received on bufferIn, with the configured frame context
    @ Sample connection: adapter.dataOut -> framer.dataIn
    output port dataOut: Svc.ComDataWithContext

    @ Port for receiving back ownership of buffers sent on dataOut
    @ Buffers are returned on bufferInReturn
    @ Sample connection: framer.dataReturnOut -> adapter.dataReturnIn
    sync input port dataReturnIn: Svc.ComDataWithContext

    # ----------------------------------------------------------------------
    # Receive side (deframer -> buffer client)
    # ----------------------------------------------------------------------

    @ Port for receiving deframed data, forwarded on bufferOut
    @ Sample connection: deframer.dataOut -> adapter.dataIn
    sync input port dataIn: Svc.ComDataWithContext

    @ Port for returning ownership of buffers received on dataIn
    @ Invoked when the buffer comes back on bufferOutReturn
    @ Sample connection: adapter.dataReturnOut -> deframer.dataReturnIn
    output port dataReturnOut: Svc.ComDataWithContext

  }

}
