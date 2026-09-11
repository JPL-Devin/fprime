    # ----------------------------------------------------------------------
    # Communications stack: TC Segment Header / MAP variant without SDLS.
    #
    #   comDriver -> frameAccumulator -> tcDeframerSeg -> tcMapReassembler
    #             -> spacePacketDeframer -> fprimeRouter
    # ----------------------------------------------------------------------

    import ComCcsds.SegmentedSubtopology

    connections RateGroups {
      rateGroup1Comp.RateGroupMemberOut[3] -> ComCcsds.SegmentedSubtopology.comQueueRun
      rateGroup1Comp.RateGroupMemberOut[4] -> ComCcsds.SegmentedSubtopology.aggregatorTimeout
      rateGroup3Comp.RateGroupMemberOut[2] -> ComCcsds.SegmentedSubtopology.bufferManagerSchedIn
      rateGroup3Comp.RateGroupMemberOut[3] -> ComCcsds.SegmentedSubtopology.tcPacketBufferManagerSchedIn
    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate      -> ComCcsds.SegmentedSubtopology.commsBufferGetCallee
      comDriver.deallocate    -> ComCcsds.SegmentedSubtopology.commsBufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                        -> ComCcsds.SegmentedSubtopology.drvReceiveIn
      ComCcsds.SegmentedSubtopology.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsds.SegmentedSubtopology.drvSendOut -> comDriver.$send
      comDriver.ready         -> ComCcsds.SegmentedSubtopology.drvConnected
    }

    connections ComCcsds_CdhCore {
      # Events and telemetry into the ComQueue
      CdhCore.Subtopology.eventsPktSend  -> ComCcsds.SegmentedSubtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      CdhCore.Subtopology.tlmSendPktSend -> ComCcsds.SegmentedSubtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router to command dispatcher
      ComCcsds.SegmentedSubtopology.commandOut -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus -> ComCcsds.SegmentedSubtopology.cmdResponseIn
    }

    connections ComCcsds_FileHandling {
      # Router to file uplink
      ComCcsds.SegmentedSubtopology.fileUplinkOut -> FileHandling.Subtopology.fileUplinkBufferSendIn
      FileHandling.Subtopology.fileUplinkBufferSendOut  -> ComCcsds.SegmentedSubtopology.fileUplinkReturnIn

      # File downlink into the ComQueue
      FileHandling.Subtopology.fileDownlinkBufferSendOut -> ComCcsds.SegmentedSubtopology.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      ComCcsds.SegmentedSubtopology.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> FileHandling.Subtopology.fileDownlinkBufferReturn
    }
