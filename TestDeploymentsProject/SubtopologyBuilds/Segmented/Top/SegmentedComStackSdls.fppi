    # ----------------------------------------------------------------------
    # Communications stack: TC Segment Header / MAP variant with SDLS.
    #
    #   comDriver -> frameAccumulator -> tcDeframerSeg -> sdlsDeframer -> saRouter
    #             -> decryptor (MAC verified) -> tcMapReassembler -> spacePacketDeframer
    #             -> fprimeRouter
    #
    # The reassembler is downstream of the SDLS SUCCESS gate: no unauthenticated
    # frame reaches it. Requires the Svc.Ccsds.AesGcmDecryptor override in
    # ../config/ComCcsdsSdlsConfig.fpp; a clear-text decryptor is signalled by
    # ComCcsdsSdls.decryptor.NullCipherInUse.
    # ----------------------------------------------------------------------

    import ComCcsdsSdls.SegmentedSubtopology

    connections RateGroups {
      rateGroup1Comp.RateGroupMemberOut[3] -> ComCcsdsSdls.SegmentedSubtopology.comQueueRun
      rateGroup1Comp.RateGroupMemberOut[4] -> ComCcsdsSdls.SegmentedSubtopology.aggregatorTimeout
      rateGroup3Comp.RateGroupMemberOut[2] -> ComCcsdsSdls.SegmentedSubtopology.bufferManagerSchedIn
      rateGroup3Comp.RateGroupMemberOut[3] -> ComCcsdsSdls.SegmentedSubtopology.tcPacketBufferManagerSchedIn
    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate      -> ComCcsdsSdls.SegmentedSubtopology.commsBufferGetCallee
      comDriver.deallocate    -> ComCcsdsSdls.SegmentedSubtopology.commsBufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                        -> ComCcsdsSdls.SegmentedSubtopology.drvReceiveIn
      ComCcsdsSdls.SegmentedSubtopology.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsdsSdls.SegmentedSubtopology.drvSendOut -> comDriver.$send
      comDriver.ready         -> ComCcsdsSdls.SegmentedSubtopology.drvConnected
    }

    connections ComCcsds_CdhCore {
      # Events and telemetry into the ComQueue
      CdhCore.Subtopology.eventsPktSend  -> ComCcsdsSdls.SegmentedSubtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      CdhCore.Subtopology.tlmSendPktSend -> ComCcsdsSdls.SegmentedSubtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router to command dispatcher
      ComCcsdsSdls.SegmentedSubtopology.commandOut -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus -> ComCcsdsSdls.SegmentedSubtopology.cmdResponseIn
    }

    connections ComCcsds_FileHandling {
      # Router to file uplink
      ComCcsdsSdls.SegmentedSubtopology.fileUplinkOut -> FileHandling.Subtopology.fileUplinkBufferSendIn
      FileHandling.Subtopology.fileUplinkBufferSendOut  -> ComCcsdsSdls.SegmentedSubtopology.fileUplinkReturnIn

      # File downlink into the ComQueue
      FileHandling.Subtopology.fileDownlinkBufferSendOut -> ComCcsdsSdls.SegmentedSubtopology.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      ComCcsdsSdls.SegmentedSubtopology.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> FileHandling.Subtopology.fileDownlinkBufferReturn
    }
