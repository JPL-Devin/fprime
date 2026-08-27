module Svc {
    @ An enumeration of queue data types
    enum QueueType : U8 { COM_QUEUE, BUFFER_QUEUE }

    @ Array of queue depths for Fw::Com types
    array ComQueueDepth = [ComQueueComPorts] U32

    @ Array of queue depths for Fw::Buffer types
    array BuffQueueDepth = [ComQueueBufferPorts] U32

    @ Component used to queue buffer types
    active component ComQueue {

      # ----------------------------------------------------------------------
      # General ports
      # ----------------------------------------------------------------------

      @ Port for emitting data ready to be sent
      output port dataOut: Svc.ComDataWithContext

      @ Port for receiving the status signal
      async input port comStatusIn: Fw.SuccessCondition

      @ Port array for receiving Fw::ComBuffers
      @ Deprecated: use comPacketQueueWithContextIn, which carries an explicit
      @ ComCfg.FrameContext instead of deriving the APID from the first
      @ FwPacketDescriptorType bytes of the buffer. Will be removed in a future release.
      async input port comPacketQueueIn: [ComQueueComPorts] Fw.Com drop

      @ Port array for receiving Fw::ComBuffers along with a ComCfg.FrameContext
      @ set by the sender (e.g. carrying the APID). Shares queues with comPacketQueueIn:
      @ port index N of either array feeds com queue N.
      async input port comPacketQueueWithContextIn: [ComQueueComPorts] Svc.ComPacketWithContext drop

      @ Port array for receiving Fw::Buffers
      @ Deprecated: use bufferQueueWithContextIn, which carries an explicit
      @ ComCfg.FrameContext instead of deriving the APID from the first
      @ FwPacketDescriptorType bytes of the buffer. Will be removed in a future release.
      async input port bufferQueueIn: [ComQueueBufferPorts] Fw.BufferSend hook

      @ Port array for receiving Fw::Buffers along with a ComCfg.FrameContext
      @ set by the sender (e.g. carrying the APID). Shares queues with bufferQueueIn:
      @ port index N of either array feeds buffer queue N. Buffer ownership is
      @ returned on bufferReturnOut[N].
      async input port bufferQueueWithContextIn: [ComQueueBufferPorts] Svc.ComDataWithContext hook

      @ Port array for returning ownership of Fw::Buffer to its original sender
      output port bufferReturnOut: [ComQueueBufferPorts] Fw.BufferSend

      @ Port for receiving Fw::Buffer whose ownership needs to be handed back
      sync input port dataReturnIn: Svc.ComDataWithContext

      @ Port for scheduling telemetry output
      async input port run: Svc.Sched drop

      # ----------------------------------------------------------------------
      # Special ports
      # ----------------------------------------------------------------------

      @ Command receive port
      command recv port CmdDisp

      @ Command registration port
      command reg port CmdReg

      @ Command response port
      command resp port CmdStatus

      @ Port for emitting events
      event port Log

      @ Port for emitting text events
      text event port LogText

      @ Port for getting the time
      time get port Time

      @ Port for emitting telemetry
      telemetry port Tlm

      include "ComQueueCommands.fppi"
      include "ComQueueEvents.fppi"
      include "ComQueueTelemetry.fppi"
    }
}
