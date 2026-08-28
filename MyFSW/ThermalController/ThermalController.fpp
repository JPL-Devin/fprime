module MyFSW {

  @ Heater state enumeration
  enum HeaterState {
    OFF     @< Heater is off
    ON      @< Heater is on
  }

  @ Thermal zone identifier
  enum ThermalZone {
    ZONE_AVIONICS   @< Avionics bay
    ZONE_BATTERY    @< Battery compartment
    ZONE_PAYLOAD    @< Payload module
  }

  @ Active component that monitors temperature sensors and controls heaters
  @ to maintain spacecraft thermal limits
  active component ThermalController {

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

    @ Set temperature limits for a thermal zone
    async command SET_TEMP_LIMITS(
      zone: MyFSW.ThermalZone @< Thermal zone to configure
      lowerLimit: F32         @< Lower temperature limit (deg C)
      upperLimit: F32         @< Upper temperature limit (deg C)
    ) opcode 0x00

    @ Manually override heater state for a thermal zone
    async command SET_HEATER(
      zone: MyFSW.ThermalZone   @< Thermal zone
      heaterState: MyFSW.HeaterState  @< Desired heater state
    ) opcode 0x01

    @ Enable or disable automatic thermal control
    async command SET_AUTO_CONTROL(
      enabled: bool @< True to enable automatic control
    ) opcode 0x02

    # ------------------------------------------------------------------
    # Telemetry
    # ------------------------------------------------------------------

    @ Current temperature of avionics zone (deg C)
    telemetry TEMP_AVIONICS: F32 id 0 format "{.2f}"

    @ Current temperature of battery zone (deg C)
    telemetry TEMP_BATTERY: F32 id 1 format "{.2f}"

    @ Current temperature of payload zone (deg C)
    telemetry TEMP_PAYLOAD: F32 id 2 format "{.2f}"

    @ Heater state for avionics zone
    telemetry HEATER_AVIONICS: MyFSW.HeaterState id 3

    @ Heater state for battery zone
    telemetry HEATER_BATTERY: MyFSW.HeaterState id 4

    @ Heater state for payload zone
    telemetry HEATER_PAYLOAD: MyFSW.HeaterState id 5

    @ Whether automatic thermal control is enabled
    telemetry AUTO_CONTROL_ENABLED: bool id 6

    # ------------------------------------------------------------------
    # Events
    # ------------------------------------------------------------------

    @ Temperature limit violation detected
    event TEMP_OUT_OF_RANGE(
      zone: MyFSW.ThermalZone @< Zone with violation
      temperature: F32        @< Current temperature
      lowerLimit: F32         @< Lower limit
      upperLimit: F32         @< Upper limit
    ) severity warning high \
      id 0 \
      format "Zone {} temperature {.2f} C outside limits [{.2f}, {.2f}]"

    @ Heater state changed
    event HEATER_STATE_CHANGED(
      zone: MyFSW.ThermalZone   @< Thermal zone
      heaterState: MyFSW.HeaterState  @< New heater state
    ) severity activity low \
      id 1 \
      format "Zone {} heater set to {}"

    @ Temperature limits updated
    event TEMP_LIMITS_SET(
      zone: MyFSW.ThermalZone @< Thermal zone
      lowerLimit: F32         @< New lower limit
      upperLimit: F32         @< New upper limit
    ) severity activity low \
      id 2 \
      format "Zone {} limits set to [{.2f}, {.2f}] C"

    @ Auto control mode changed
    event AUTO_CONTROL_SET(
      enabled: bool @< New state
    ) severity activity low \
      id 3 \
      format "Automatic thermal control set to {}"

    @ Invalid temperature limits specified
    event INVALID_LIMITS(
      lowerLimit: F32 @< Specified lower limit
      upperLimit: F32 @< Specified upper limit
    ) severity warning low \
      id 4 \
      format "Invalid limits: lower {.2f} >= upper {.2f}"

    # ------------------------------------------------------------------
    # Interfaces
    # ------------------------------------------------------------------
    import Fw.Event
    import Fw.Command
    import Fw.Channel

  }

}
