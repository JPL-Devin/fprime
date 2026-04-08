// ======================================================================
// \title  ThermalController.hpp
// \brief  hpp file for ThermalController component implementation class
// ======================================================================

#ifndef MyFSW_ThermalController_HPP
#define MyFSW_ThermalController_HPP

#include "MyFSW/ThermalController/ThermalControllerComponentAc.hpp"

namespace MyFSW {

class ThermalController final : public ThermalControllerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct a ThermalController
    ThermalController(const char* compName);

    //! Destroy a ThermalController
    ~ThermalController();

  private:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Number of thermal zones
    static constexpr U32 NUM_ZONES = 3;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler for SET_TEMP_LIMITS command
    void SET_TEMP_LIMITS_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        MyFSW::ThermalZone zone,
        F32 lowerLimit,
        F32 upperLimit
    ) final;

    //! Handler for SET_HEATER command
    void SET_HEATER_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        MyFSW::ThermalZone zone,
        MyFSW::HeaterState state
    ) final;

    //! Handler for SET_AUTO_CONTROL command
    void SET_AUTO_CONTROL_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        bool enabled
    ) final;

    // ----------------------------------------------------------------------
    // Handler implementations for input ports
    // ----------------------------------------------------------------------

    //! Handler for schedIn port
    void schedIn_handler(
        FwIndexType portNum,
        U32 context
    ) final;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Simulate reading a temperature sensor for the given zone
    F32 readTemperature(U32 zoneIndex);

    //! Apply automatic thermal control logic
    void applyThermalControl(U32 zoneIndex, F32 temperature);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Current temperature readings per zone
    F32 m_temperatures[NUM_ZONES];

    //! Lower temperature limits per zone (deg C)
    F32 m_lowerLimits[NUM_ZONES];

    //! Upper temperature limits per zone (deg C)
    F32 m_upperLimits[NUM_ZONES];

    //! Heater states per zone
    HeaterState m_heaterStates[NUM_ZONES];

    //! Whether automatic thermal control is enabled
    bool m_autoControlEnabled;

    //! Tick counter for temperature simulation
    U32 m_tickCount;
};

}  // namespace MyFSW

#endif
