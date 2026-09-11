    # ----------------------------------------------------------------------
    # Communications stack: ComCcsds.SegmentedSubtopology and every connection to it
    # ----------------------------------------------------------------------
    instance ComCcsds.SegmentedSubtopology

    connections RateGroups {
      rateGroup1Comp.RateGroupMemberOut[5] -> ComCcsds.SegmentedSubtopology.comQueueRun
      rateGroup1Comp.RateGroupMemberOut[7] -> ComCcsds.SegmentedSubtopology.aggregatorTimeout
      rateGroup3Comp.RateGroupMemberOut[3] -> ComCcsds.SegmentedSubtopology.bufferManagerSchedIn
      rateGroup3Comp.RateGroupMemberOut[9] -> ComCcsds.SegmentedSubtopology.tcPacketBufferManagerSchedIn
    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate   -> ComCcsds.SegmentedSubtopology.commsBufferGetCallee
      comDriver.deallocate -> ComCcsds.SegmentedSubtopology.commsBufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                          -> ComCcsds.SegmentedSubtopology.drvReceiveIn
      ComCcsds.SegmentedSubtopology.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsds.SegmentedSubtopology.drvSendOut -> comDriver.$send
      comDriver.ready                 -> ComCcsds.SegmentedSubtopology.drvConnected
    }

    connections ComCcsds_CdhCore {
      # Events and telemetry to comQueue
      CdhCore.Subtopology.eventsPktSend  -> ComCcsds.SegmentedSubtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      CdhCore.Subtopology.tlmSendPktSend -> ComCcsds.SegmentedSubtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router <-> CmdDispatcher
      ComCcsds.SegmentedSubtopology.commandOut        -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> ComCcsds.SegmentedSubtopology.cmdResponseIn
      cmdSeq.comCmdOut                       -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> cmdSeq.cmdResponseIn
      wasmSeq.cmdOut                         -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> wasmSeq.cmdResponseIn
    }

    connections ComCcsds_FileHandling {
      # File Downlink <-> ComQueue
      FileHandling.Subtopology.fileDownlinkBufferSendOut -> ComCcsds.SegmentedSubtopology.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      ComCcsds.SegmentedSubtopology.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> FileHandling.Subtopology.fileDownlinkBufferReturn

      # Router <-> FileUplink
      ComCcsds.SegmentedSubtopology.fileUplinkOut                    -> FileHandling.Subtopology.fileUplinkBufferSendIn
      FileHandling.Subtopology.fileUplinkBufferSendOut     -> ComCcsds.SegmentedSubtopology.fileUplinkReturnIn
    }
