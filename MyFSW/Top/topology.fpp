module MyFSW {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup1
    rateGroup2
  }

  topology MyFSW {

    # ----------------------------------------------------------------------
    # Subtopology instances (standard F Prime services)
    # ----------------------------------------------------------------------
    instance CdhCore.Subtopology
    instance ComFprime.Subtopology
    instance FileHandling.Subtopology

    # ----------------------------------------------------------------------
    # Instances used in the topology
    # ----------------------------------------------------------------------

    # Mission-specific components
    instance thermalController
    instance navSensor
    instance powerManager

    # Rate group management
    instance rateGroup1Comp
    instance rateGroup2Comp
    instance rateGroupDriverComp
    instance linuxTimer

    # Sequencing
    instance cmdSeq

    # Communications driver
    instance comDriver

    # Time
    instance posixTime

    # System monitoring
    instance systemResources

    # ----------------------------------------------------------------------
    # Pattern graph specifiers
    # ----------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp

    event connections instance CdhCore.events

    telemetry connections instance CdhCore.tlmSend

    text event connections instance CdhCore.textLogger

    time connections instance posixTime

    health connections instance CdhCore.$health

    param connections instance FileHandling.prmDb

    # ----------------------------------------------------------------------
    # Direct graph specifiers
    # ----------------------------------------------------------------------

    connections RateGroups {
      # Linux timer drives the rate group driver
      linuxTimer.CycleOut -> rateGroupDriverComp.CycleIn

      # Rate group 1: 1 Hz — core telemetry and mission components
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup1] -> rateGroup1Comp.CycleIn
      rateGroup1Comp.RateGroupMemberOut[0] -> thermalController.schedIn
      rateGroup1Comp.RateGroupMemberOut[1] -> navSensor.schedIn
      rateGroup1Comp.RateGroupMemberOut[2] -> powerManager.schedIn
      rateGroup1Comp.RateGroupMemberOut[3] -> CdhCore.Subtopology.tlmSendRun
      rateGroup1Comp.RateGroupMemberOut[4] -> FileHandling.Subtopology.fileDownlinkRun
      rateGroup1Comp.RateGroupMemberOut[5] -> systemResources.run
      rateGroup1Comp.RateGroupMemberOut[6] -> ComFprime.Subtopology.comQueueRun
      rateGroup1Comp.RateGroupMemberOut[7] -> CdhCore.Subtopology.cmdDispRun

      # Rate group 2: 0.5 Hz — housekeeping and sequencing
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup2] -> rateGroup2Comp.CycleIn
      rateGroup2Comp.RateGroupMemberOut[0] -> cmdSeq.schedIn
      rateGroup2Comp.RateGroupMemberOut[1] -> CdhCore.Subtopology.healthRun
      rateGroup2Comp.RateGroupMemberOut[2] -> FileHandling.Subtopology.fileManagerSchedIn
    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate -> ComFprime.Subtopology.commsBufferGetCallee
      comDriver.deallocate -> ComFprime.Subtopology.commsBufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv -> ComFprime.Subtopology.drvReceiveIn
      ComFprime.Subtopology.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComFprime.Subtopology.drvSendOut -> comDriver.$send
      comDriver.ready -> ComFprime.Subtopology.drvConnected
    }

    connections ComFprime_CdhCore {
      # Events and telemetry to comQueue
      CdhCore.Subtopology.eventsPktSend -> ComFprime.Subtopology.comPacketQueueIn[ComFprime.Ports_ComPacketQueue.EVENTS]
      CdhCore.Subtopology.tlmSendPktSend -> ComFprime.Subtopology.comPacketQueueIn[ComFprime.Ports_ComPacketQueue.TELEMETRY]

      # Router <-> CmdDispatcher
      ComFprime.Subtopology.commandOut -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus -> ComFprime.Subtopology.cmdResponseIn
      cmdSeq.comCmdOut -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus -> cmdSeq.cmdResponseIn
    }

    connections ComFprime_FileHandling {
      # File Downlink <-> ComQueue
      FileHandling.Subtopology.fileDownlinkBufferSendOut -> ComFprime.Subtopology.bufferQueueIn[ComFprime.Ports_ComBufferQueue.FILE]
      ComFprime.Subtopology.bufferReturnOut[ComFprime.Ports_ComBufferQueue.FILE] -> FileHandling.Subtopology.fileDownlinkBufferReturn

      # Router <-> FileUplink
      ComFprime.Subtopology.fileUplinkOut -> FileHandling.Subtopology.fileUplinkBufferSendIn
      FileHandling.Subtopology.fileUplinkBufferSendOut -> ComFprime.Subtopology.fileUplinkReturnIn
    }

  }

}
