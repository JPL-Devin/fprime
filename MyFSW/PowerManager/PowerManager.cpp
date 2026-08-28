// ======================================================================
// \title  PowerManager.cpp
// \brief  cpp file for PowerManager component implementation class
// ======================================================================

#include "PowerManager.hpp"
#include <cmath>

namespace MyFSW {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PowerManager::PowerManager(const char* compName)
    : PowerManagerComponentBase(compName),
      m_powerState(PowerState::NOMINAL),
      m_busEnabled(),
      m_energyConsumed(0.0f),
      m_tickCount(0),
      m_batterySOC(95.0f)
{
    // All buses enabled by default
    for (U32 i = 0; i < NUM_BUSES; i++) {
        m_busEnabled[i] = true;
    }
}

PowerManager::~PowerManager() {}

// ----------------------------------------------------------------------
// Handler implementations for input ports
// ----------------------------------------------------------------------

void PowerManager::schedIn_handler(
    FwIndexType portNum,
    U32 context)
{
    m_tickCount++;

    // Read battery voltage
    F32 voltage = this->readBatteryVoltage();

    // Read bus currents
    F32 busPrimaryCurrent = this->readBusCurrent(0);
    F32 busPayloadCurrent = this->readBusCurrent(1);
    F32 busHeaterCurrent = this->readBusCurrent(2);

    // Calculate total power
    F32 totalCurrent = busPrimaryCurrent + busPayloadCurrent + busHeaterCurrent;
    F32 totalPower = voltage * totalCurrent;

    // Accumulate energy (assuming 1 Hz rate group => 1 second per tick)
    m_energyConsumed += totalPower / 3600.0f;  // Convert W*s to Wh

    // Simulate slow battery drain
    m_batterySOC -= 0.001f;
    if (m_batterySOC < 0.0f) {
        m_batterySOC = 0.0f;
    }

    // Check voltage thresholds
    this->checkVoltageThresholds(voltage);

    // Emit telemetry
    this->tlmWrite_BATTERY_VOLTAGE(voltage);
    this->tlmWrite_BATTERY_SOC(m_batterySOC);
    this->tlmWrite_TOTAL_POWER(totalPower);
    this->tlmWrite_POWER_STATE(m_powerState);
    this->tlmWrite_BUS_PRIMARY_CURRENT(busPrimaryCurrent);
    this->tlmWrite_BUS_PAYLOAD_CURRENT(busPayloadCurrent);
    this->tlmWrite_BUS_HEATER_CURRENT(busHeaterCurrent);
    this->tlmWrite_ENERGY_CONSUMED(m_energyConsumed);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PowerManager::SET_POWER_STATE_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    MyFSW::PowerState powerState)
{
    PowerState oldState = m_powerState;
    m_powerState = powerState;
    this->log_ACTIVITY_HI_POWER_STATE_CHANGED(oldState, powerState);

    // If entering low power or critical, shed non-essential loads
    if (powerState == PowerState::LOW_POWER || powerState == PowerState::CRITICAL) {
        m_busEnabled[1] = false;  // Disable payload bus
        this->log_ACTIVITY_LO_BUS_STATE_CHANGED(PowerBus::BUS_PAYLOAD, false);
    }

    if (powerState == PowerState::CRITICAL || powerState == PowerState::SAFE_MODE) {
        m_busEnabled[2] = false;  // Disable heater bus
        this->log_ACTIVITY_LO_BUS_STATE_CHANGED(PowerBus::BUS_HEATER, false);
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void PowerManager::SET_BUS_ENABLE_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    MyFSW::PowerBus bus,
    bool enabled)
{
    U32 busIndex = static_cast<U32>(bus.e);
    if (busIndex < NUM_BUSES) {
        m_busEnabled[busIndex] = enabled;
        this->log_ACTIVITY_LO_BUS_STATE_CHANGED(bus, enabled);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
    }
}

void PowerManager::RESET_ENERGY_COUNTER_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq)
{
    m_energyConsumed = 0.0f;
    this->log_ACTIVITY_LO_ENERGY_COUNTER_RESET();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

F32 PowerManager::readBatteryVoltage() {
    // Simulate battery voltage based on state of charge
    // Li-ion typical: 4.2V full -> 3.0V empty per cell, 3S pack
    F32 cellVoltage = 3.0f + (m_batterySOC / 100.0f) * 1.2f;
    F32 packVoltage = cellVoltage * 3.0f;  // 3S configuration

    // Add small ripple
    packVoltage += 0.05f * std::sin(static_cast<F32>(m_tickCount) * 0.1f);

    return packVoltage;
}

F32 PowerManager::readBusCurrent(U32 busIndex) {
    if (busIndex >= NUM_BUSES || !m_busEnabled[busIndex]) {
        return 0.0f;
    }

    // Simulate typical current draw per bus
    F32 baseCurrent = 0.0f;
    switch (busIndex) {
        case 0:  // Primary: avionics, always on
            baseCurrent = 0.8f;
            break;
        case 1:  // Payload
            baseCurrent = 1.2f;
            break;
        case 2:  // Heater
            baseCurrent = 0.5f;
            break;
        default:
            break;
    }

    // Add small variation
    baseCurrent += 0.02f * std::sin(static_cast<F32>(m_tickCount) * 0.05f + static_cast<F32>(busIndex));

    return baseCurrent;
}

void PowerManager::checkVoltageThresholds(F32 voltage) {
    if (voltage < VOLTAGE_CRITICAL_THRESHOLD && m_powerState != PowerState::SAFE_MODE) {
        PowerState oldState = m_powerState;
        m_powerState = PowerState::SAFE_MODE;
        this->log_FATAL_BATTERY_CRITICAL(voltage);
        this->log_ACTIVITY_HI_POWER_STATE_CHANGED(oldState, m_powerState);
    } else if (voltage < VOLTAGE_LOW_THRESHOLD && m_powerState == PowerState::NOMINAL) {
        PowerState oldState = m_powerState;
        m_powerState = PowerState::LOW_POWER;
        this->log_WARNING_HI_BATTERY_LOW(voltage, VOLTAGE_LOW_THRESHOLD);
        this->log_ACTIVITY_HI_POWER_STATE_CHANGED(oldState, m_powerState);
    }
}

}  // namespace MyFSW
