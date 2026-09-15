// ======================================================================
// \title  ThermalController.cpp
// \brief  cpp file for ThermalController component implementation class
// ======================================================================

#include "ThermalController.hpp"
#include <cmath>

namespace MyFSW {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ThermalController::ThermalController(const char* compName)
    : ThermalControllerComponentBase(compName),
      m_temperatures(),
      m_lowerLimits(),
      m_upperLimits(),
      m_heaterStates(),
      m_autoControlEnabled(true),
      m_tickCount(0)
{
    // Initialize default temperature limits (deg C)
    // Avionics: -20 to +50
    m_lowerLimits[0] = -20.0f;
    m_upperLimits[0] = 50.0f;
    // Battery: 0 to +40
    m_lowerLimits[1] = 0.0f;
    m_upperLimits[1] = 40.0f;
    // Payload: -10 to +45
    m_lowerLimits[2] = -10.0f;
    m_upperLimits[2] = 45.0f;

    // Initialize temperatures to mid-range
    for (U32 i = 0; i < NUM_ZONES; i++) {
        m_temperatures[i] = (m_lowerLimits[i] + m_upperLimits[i]) / 2.0f;
        m_heaterStates[i] = HeaterState::OFF;
    }
}

ThermalController::~ThermalController() {}

// ----------------------------------------------------------------------
// Handler implementations for input ports
// ----------------------------------------------------------------------

void ThermalController::schedIn_handler(
    FwIndexType portNum,
    U32 context)
{
    m_tickCount++;

    // Read temperatures and apply control for each zone
    for (U32 i = 0; i < NUM_ZONES; i++) {
        m_temperatures[i] = this->readTemperature(i);

        if (m_autoControlEnabled) {
            this->applyThermalControl(i, m_temperatures[i]);
        }
    }

    // Emit telemetry
    this->tlmWrite_TEMP_AVIONICS(m_temperatures[0]);
    this->tlmWrite_TEMP_BATTERY(m_temperatures[1]);
    this->tlmWrite_TEMP_PAYLOAD(m_temperatures[2]);
    this->tlmWrite_HEATER_AVIONICS(m_heaterStates[0]);
    this->tlmWrite_HEATER_BATTERY(m_heaterStates[1]);
    this->tlmWrite_HEATER_PAYLOAD(m_heaterStates[2]);
    this->tlmWrite_AUTO_CONTROL_ENABLED(m_autoControlEnabled);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ThermalController::SET_TEMP_LIMITS_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    MyFSW::ThermalZone zone,
    F32 lowerLimit,
    F32 upperLimit)
{
    // Validate limits
    if (lowerLimit >= upperLimit) {
        this->log_WARNING_LO_INVALID_LIMITS(lowerLimit, upperLimit);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    U32 zoneIndex = static_cast<U32>(zone.e);
    if (zoneIndex < NUM_ZONES) {
        m_lowerLimits[zoneIndex] = lowerLimit;
        m_upperLimits[zoneIndex] = upperLimit;
        this->log_ACTIVITY_LO_TEMP_LIMITS_SET(zone, lowerLimit, upperLimit);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
    }
}

void ThermalController::SET_HEATER_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    MyFSW::ThermalZone zone,
    MyFSW::HeaterState heaterState)
{
    U32 zoneIndex = static_cast<U32>(zone.e);
    if (zoneIndex < NUM_ZONES) {
        m_heaterStates[zoneIndex] = heaterState;
        this->log_ACTIVITY_LO_HEATER_STATE_CHANGED(zone, heaterState);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
    }
}

void ThermalController::SET_AUTO_CONTROL_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    bool enabled)
{
    m_autoControlEnabled = enabled;
    this->log_ACTIVITY_LO_AUTO_CONTROL_SET(enabled);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

F32 ThermalController::readTemperature(U32 zoneIndex) {
    // Simulate temperature reading with a slow sinusoidal drift
    // In real FSW, this would read from an ADC or I2C temperature sensor
    F32 baseTemp = (m_lowerLimits[zoneIndex] + m_upperLimits[zoneIndex]) / 2.0f;
    F32 amplitude = (m_upperLimits[zoneIndex] - m_lowerLimits[zoneIndex]) / 4.0f;
    F32 phase = static_cast<F32>(zoneIndex) * 2.094f;  // 120 degrees offset per zone
    F32 drift = amplitude * std::sin(static_cast<F32>(m_tickCount) * 0.01f + phase);

    // Heater effect: if heater is on, temperature trends upward
    if (m_heaterStates[zoneIndex] == HeaterState::ON) {
        drift += 2.0f;
    }

    return baseTemp + drift;
}

void ThermalController::applyThermalControl(U32 zoneIndex, F32 temperature) {
    ThermalZone zone = static_cast<ThermalZone::T>(zoneIndex);

    // Check if temperature is out of range
    if (temperature < m_lowerLimits[zoneIndex] || temperature > m_upperLimits[zoneIndex]) {
        this->log_WARNING_HI_TEMP_OUT_OF_RANGE(
            zone,
            temperature,
            m_lowerLimits[zoneIndex],
            m_upperLimits[zoneIndex]
        );
    }

    // Hysteresis-based heater control:
    // Turn on heater if below lower limit + 2 deg margin
    // Turn off heater if above lower limit + 5 deg margin
    F32 heaterOnThreshold = m_lowerLimits[zoneIndex] + 2.0f;
    F32 heaterOffThreshold = m_lowerLimits[zoneIndex] + 5.0f;

    HeaterState previousState = m_heaterStates[zoneIndex];

    if (temperature < heaterOnThreshold) {
        m_heaterStates[zoneIndex] = HeaterState::ON;
    } else if (temperature > heaterOffThreshold) {
        m_heaterStates[zoneIndex] = HeaterState::OFF;
    }

    // Log state change
    if (m_heaterStates[zoneIndex] != previousState) {
        this->log_ACTIVITY_LO_HEATER_STATE_CHANGED(zone, m_heaterStates[zoneIndex]);
    }
}

}  // namespace MyFSW
