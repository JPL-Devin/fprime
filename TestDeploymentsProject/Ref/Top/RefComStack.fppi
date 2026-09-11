    # ----------------------------------------------------------------------
    # Communications stack: ComCcsds.Subtopology and every connection to it
    # ----------------------------------------------------------------------
    instance ComCcsds.Subtopology

    connections RateGroups {
      rateGroup1Comp.RateGroupMemberOut[5] -> ComCcsds.Subtopology.comQueueRun
      rateGroup1Comp.RateGroupMemberOut[7] -> ComCcsds.Subtopology.aggregatorTimeout
      rateGroup3Comp.RateGroupMemberOut[3] -> ComCcsds.Subtopology.bufferManagerSchedIn
    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate   -> ComCcsds.Subtopology.commsBufferGetCallee
      comDriver.deallocate -> ComCcsds.Subtopology.commsBufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                          -> ComCcsds.Subtopology.drvReceiveIn
      ComCcsds.Subtopology.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsds.Subtopology.drvSendOut -> comDriver.$send
      comDriver.ready                 -> ComCcsds.Subtopology.drvConnected
    }

    connections ComCcsds_CdhCore {
      # Events and telemetry to comQueue
      CdhCore.Subtopology.eventsPktSend  -> ComCcsds.Subtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      CdhCore.Subtopology.tlmSendPktSend -> ComCcsds.Subtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router <-> CmdDispatcher
      ComCcsds.Subtopology.commandOut        -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> ComCcsds.Subtopology.cmdResponseIn
      cmdSeq.comCmdOut                       -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> cmdSeq.cmdResponseIn
      wasmSeq.cmdOut                         -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> wasmSeq.cmdResponseIn
    }

    connections ComCcsds_FileHandling {
      # File Downlink <-> ComQueue
      FileHandling.Subtopology.fileDownlinkBufferSendOut -> ComCcsds.Subtopology.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      ComCcsds.Subtopology.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> FileHandling.Subtopology.fileDownlinkBufferReturn

      # Router <-> FileUplink
      ComCcsds.Subtopology.fileUplinkOut                    -> FileHandling.Subtopology.fileUplinkBufferSendIn
      FileHandling.Subtopology.fileUplinkBufferSendOut     -> ComCcsds.Subtopology.fileUplinkReturnIn
    }
