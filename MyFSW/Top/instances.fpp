module MyFSW {

  # ----------------------------------------------------------------------
  # Defaults
  # ----------------------------------------------------------------------

  module Default {
    constant QUEUE_SIZE = 10
    constant STACK_SIZE = 64 * 1024
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  instance thermalController: MyFSW.ThermalController base id 0x10000000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 40

  instance rateGroup1Comp: Svc.ActiveRateGroup base id 0x10001000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 43

  instance rateGroup2Comp: Svc.ActiveRateGroup base id 0x10002000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 42

  instance cmdSeq: Svc.CmdSequencer base id 0x10005000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 20

  # ----------------------------------------------------------------------
  # Queued component instances
  # ----------------------------------------------------------------------

  instance navSensor: MyFSW.NavSensor base id 0x10010000 \
    queue size Default.QUEUE_SIZE

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance powerManager: MyFSW.PowerManager base id 0x10020000

  instance posixTime: Svc.PosixTime base id 0x10021000

  instance rateGroupDriverComp: Svc.RateGroupDriver base id 0x10022000

  instance linuxTimer: Svc.LinuxTimer base id 0x10023000

  instance comDriver: Drv.TcpClient base id 0x10024000

  instance systemResources: Svc.SystemResources base id 0x1002A000

}
