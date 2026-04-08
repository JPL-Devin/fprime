module MyFSW {

  @ Spacecraft power state
  enum PowerState {
    NOMINAL       @< All systems nominal
    LOW_POWER     @< Battery below threshold, non-essential loads shed
    CRITICAL      @< Battery critically low, only essential systems active
    SAFE_MODE     @< Minimum power configuration for survival
  }

  @ Power bus identifier
  enum PowerBus {
    BUS_PRIMARY    @< Primary power bus
    BUS_PAYLOAD    @< Payload power bus
    BUS_HEATER     @< Heater power bus
  }

  @ Passive component that monitors spacecraft power state,
  @ battery voltage, and power bus status
  passive component PowerManager {

    # ------------------------------------------------------------------
    # General Ports
    # ------------------------------------------------------------------

    @ Schedule input port driven by rate group
    sync input port schedIn: Svc.Sched

    @ Time get port
    time get port timeCaller

    # ------------------------------------------------------------------
    # Commands
    # ------------------------------------------------------------------

    @ Set the power state manually
    sync command SET_POWER_STATE(
      powerState: MyFSW.PowerState @< Desired power state
    ) opcode 0x00

    @ Enable or disable a power bus
    sync command SET_BUS_ENABLE(
      bus: MyFSW.PowerBus @< Power bus to control
      enabled: bool       @< True to enable, false to disable
    ) opcode 0x01

    @ Reset the energy consumed counter
    sync command RESET_ENERGY_COUNTER opcode 0x02

    # ------------------------------------------------------------------
    # Telemetry
    # ------------------------------------------------------------------

    @ Battery voltage (V)
    telemetry BATTERY_VOLTAGE: F32 id 0 format "{.2f}"

    @ Battery state of charge (percent)
    telemetry BATTERY_SOC: F32 id 1 format "{.1f}"

    @ Total power consumption (W)
    telemetry TOTAL_POWER: F32 id 2 format "{.2f}"

    @ Current power state
    telemetry POWER_STATE: MyFSW.PowerState id 3

    @ Primary bus current (A)
    telemetry BUS_PRIMARY_CURRENT: F32 id 4 format "{.3f}"

    @ Payload bus current (A)
    telemetry BUS_PAYLOAD_CURRENT: F32 id 5 format "{.3f}"

    @ Heater bus current (A)
    telemetry BUS_HEATER_CURRENT: F32 id 6 format "{.3f}"

    @ Cumulative energy consumed (Wh)
    telemetry ENERGY_CONSUMED: F32 id 7 format "{.2f}"

    # ------------------------------------------------------------------
    # Events
    # ------------------------------------------------------------------

    @ Power state transition
    event POWER_STATE_CHANGED(
      oldState: MyFSW.PowerState @< Previous state
      newState: MyFSW.PowerState @< New state
    ) severity activity high \
      id 0 \
      format "Power state changed from {} to {}"

    @ Battery voltage low warning
    event BATTERY_LOW(
      voltage: F32 @< Current voltage
      threshold: F32 @< Threshold voltage
    ) severity warning high \
      id 1 \
      format "Battery voltage {.2f}V below threshold {.2f}V"

    @ Battery voltage critical
    event BATTERY_CRITICAL(
      voltage: F32 @< Current voltage
    ) severity fatal \
      id 2 \
      format "CRITICAL: Battery voltage {.2f}V — entering safe mode"

    @ Power bus state changed
    event BUS_STATE_CHANGED(
      bus: MyFSW.PowerBus @< Power bus
      enabled: bool       @< New state
    ) severity activity low \
      id 3 \
      format "Power bus {} set to enabled={}"

    @ Energy counter reset
    event ENERGY_COUNTER_RESET \
      severity activity low \
      id 4 \
      format "Energy consumed counter reset to zero"

    # ------------------------------------------------------------------
    # Interfaces
    # ------------------------------------------------------------------
    import Fw.Event
    import Fw.Command
    import Fw.Channel

  }

}
