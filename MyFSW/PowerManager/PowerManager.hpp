// ======================================================================
// \title  PowerManager.hpp
// \brief  hpp file for PowerManager component implementation class
// ======================================================================

#ifndef MyFSW_PowerManager_HPP
#define MyFSW_PowerManager_HPP

#include "MyFSW/PowerManager/PowerManagerComponentAc.hpp"

namespace MyFSW {

class PowerManager final : public PowerManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct a PowerManager
    PowerManager(const char* compName);

    //! Destroy a PowerManager
    ~PowerManager();

  private:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Number of power buses
    static constexpr U32 NUM_BUSES = 3;

    //! Battery voltage thresholds
    static constexpr F32 VOLTAGE_LOW_THRESHOLD = 11.0f;
    static constexpr F32 VOLTAGE_CRITICAL_THRESHOLD = 10.0f;
    static constexpr F32 VOLTAGE_NOMINAL = 12.6f;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler for SET_POWER_STATE command
    void SET_POWER_STATE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        MyFSW::PowerState state
    ) final;

    //! Handler for SET_BUS_ENABLE command
    void SET_BUS_ENABLE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        MyFSW::PowerBus bus,
        bool enabled
    ) final;

    //! Handler for RESET_ENERGY_COUNTER command
    void RESET_ENERGY_COUNTER_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq
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

    //! Simulate battery voltage reading
    F32 readBatteryVoltage();

    //! Simulate bus current reading
    F32 readBusCurrent(U32 busIndex);

    //! Check voltage thresholds and update power state
    void checkVoltageThresholds(F32 voltage);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Current power state
    PowerState m_powerState;

    //! Bus enable flags
    bool m_busEnabled[NUM_BUSES];

    //! Cumulative energy consumed (Wh)
    F32 m_energyConsumed;

    //! Tick counter for simulation
    U32 m_tickCount;

    //! Simulated battery state of charge (0-100%)
    F32 m_batterySOC;
};

}  // namespace MyFSW

#endif
