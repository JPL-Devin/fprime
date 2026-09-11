    # ----------------------------------------------------------------------
    # Core instances and connections shared by the non-SDLS (topology.fpp)
    # and SDLS (topologySdls.fpp) Segmented topologies.
    # ----------------------------------------------------------------------

    # ----------------------------------------------------------------------
    # Subtopology imports
    # ----------------------------------------------------------------------

    import CdhCore.Subtopology
    import FileHandling.Subtopology

    # ----------------------------------------------------------------------
    # Instances used in the topology
    # ----------------------------------------------------------------------

    instance posixTime
    instance rateGroup1Comp
    instance rateGroup2Comp
    instance rateGroup3Comp
    instance rateGroupDriverComp
    instance linuxTimer
    instance comDriver

    # ----------------------------------------------------------------------
    # Pattern graph specifiers
    # ----------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp
    event connections instance CdhCore.events
    param connections instance FileHandling.prmDb
    telemetry connections instance CdhCore.tlmSend
    text event connections instance CdhCore.textLogger
    time connections instance posixTime
    health connections instance CdhCore.$health

    # ----------------------------------------------------------------------
    # Direct graph specifiers
    # ----------------------------------------------------------------------

    connections RateGroups {
      # Block driver
      linuxTimer.CycleOut -> rateGroupDriverComp.CycleIn

      # Rate group 1
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup1] -> rateGroup1Comp.CycleIn
      rateGroup1Comp.RateGroupMemberOut[0] -> CdhCore.Subtopology.tlmSendRun
      rateGroup1Comp.RateGroupMemberOut[1] -> FileHandling.Subtopology.fileDownlinkRun
      rateGroup1Comp.RateGroupMemberOut[2] -> CdhCore.Subtopology.cmdDispRun

      # Rate group 2
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup2] -> rateGroup2Comp.CycleIn
      rateGroup2Comp.RateGroupMemberOut[0] -> FileHandling.Subtopology.fileManagerSchedIn

      # Rate group 3
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup3] -> rateGroup3Comp.CycleIn
      rateGroup3Comp.RateGroupMemberOut[0] -> CdhCore.Subtopology.healthRun
      rateGroup3Comp.RateGroupMemberOut[1] -> CdhCore.Subtopology.eventsRun
    }
