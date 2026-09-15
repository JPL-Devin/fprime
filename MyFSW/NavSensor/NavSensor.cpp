// ======================================================================
// \title  NavSensor.cpp
// \brief  cpp file for NavSensor component implementation class
// ======================================================================

#include "NavSensor.hpp"
#include <cmath>
#include <cstdlib>

namespace MyFSW {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

NavSensor::NavSensor(const char* compName)
    : NavSensorComponentBase(compName),
      m_mode(NavMode::IDLE),
      m_gyroBias(),
      m_sampleCount(0),
      m_tickCount(0)
{
    // Initialize gyro bias to small simulated values
    m_gyroBias[0] = 0.012f;
    m_gyroBias[1] = -0.008f;
    m_gyroBias[2] = 0.005f;
}

NavSensor::~NavSensor() {}

// ----------------------------------------------------------------------
// Handler implementations for input ports
// ----------------------------------------------------------------------

void NavSensor::schedIn_handler(
    FwIndexType portNum,
    U32 context)
{
    // Dispatch queued commands/messages
    this->doDispatch();

    // Only sample when not in IDLE mode
    if (m_mode == NavMode::IDLE) {
        return;
    }

    m_tickCount++;
    m_sampleCount++;

    // Read all sensors
    Vector3 gyroData = this->readGyroscope();
    Vector3 accelData = this->readAccelerometer();
    Vector3 magData = this->readMagnetometer();
    F32 sensorTemp = this->readSensorTemp();

    // Emit telemetry
    this->tlmWrite_GYRO_RATES(gyroData);
    this->tlmWrite_ACCEL_DATA(accelData);
    this->tlmWrite_MAG_DATA(magData);
    this->tlmWrite_NAV_MODE(m_mode);
    this->tlmWrite_SAMPLE_COUNT(m_sampleCount);
    this->tlmWrite_SENSOR_TEMP(sensorTemp);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void NavSensor::SET_NAV_MODE_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    MyFSW::NavMode mode)
{
    NavMode oldMode = m_mode;
    m_mode = mode;
    this->log_ACTIVITY_HI_NAV_MODE_CHANGED(oldMode, mode);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void NavSensor::ZERO_GYRO_BIAS_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq)
{
    // Log the bias being removed
    this->log_ACTIVITY_LO_GYRO_BIAS_ZEROED(
        m_gyroBias[0],
        m_gyroBias[1],
        m_gyroBias[2]
    );

    // Zero the bias
    m_gyroBias[0] = 0.0f;
    m_gyroBias[1] = 0.0f;
    m_gyroBias[2] = 0.0f;

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void NavSensor::SELF_TEST_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq)
{
    // Simulate a self-test by reading sensors and verifying ranges
    Vector3 accel = this->readAccelerometer();

    // Check that accelerometer reads approximately 1g magnitude
    F32 accelMag = std::sqrt(
        accel.get_x() * accel.get_x() +
        accel.get_y() * accel.get_y() +
        accel.get_z() * accel.get_z()
    );

    // Accept if within 20% of 9.81 m/s^2
    if (accelMag > 7.85f && accelMag < 11.77f) {
        this->log_ACTIVITY_HI_SELF_TEST_PASSED();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->log_WARNING_HI_SELF_TEST_FAILED(1);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

Vector3 NavSensor::readGyroscope() {
    // Simulate gyroscope readings (deg/s)
    // Small oscillation around zero + bias, representing slow tumble
    F32 t = static_cast<F32>(m_tickCount) * 0.02f;
    F32 noiseScale = (m_mode == NavMode::HIGH_RATE) ? 0.001f : 0.01f;

    F32 x = 0.5f * std::sin(t * 0.3f) + m_gyroBias[0] +
            noiseScale * static_cast<F32>(std::rand() % 100 - 50) / 50.0f;
    F32 y = 0.3f * std::cos(t * 0.2f) + m_gyroBias[1] +
            noiseScale * static_cast<F32>(std::rand() % 100 - 50) / 50.0f;
    F32 z = 0.1f * std::sin(t * 0.5f) + m_gyroBias[2] +
            noiseScale * static_cast<F32>(std::rand() % 100 - 50) / 50.0f;

    return Vector3(x, y, z);
}

Vector3 NavSensor::readAccelerometer() {
    // Simulate accelerometer (m/s^2)
    // Primarily 1g in Z-axis with small perturbations
    F32 noiseScale = 0.05f;

    F32 x = noiseScale * static_cast<F32>(std::rand() % 100 - 50) / 50.0f;
    F32 y = noiseScale * static_cast<F32>(std::rand() % 100 - 50) / 50.0f;
    F32 z = 9.81f + noiseScale * static_cast<F32>(std::rand() % 100 - 50) / 50.0f;

    return Vector3(x, y, z);
}

Vector3 NavSensor::readMagnetometer() {
    // Simulate magnetometer readings (micro-Tesla)
    // Earth's field ~25-65 uT depending on location
    F32 t = static_cast<F32>(m_tickCount) * 0.02f;

    F32 x = 20.0f * std::cos(t * 0.1f);
    F32 y = 5.0f * std::sin(t * 0.1f);
    F32 z = 40.0f + 2.0f * std::sin(t * 0.05f);

    return Vector3(x, y, z);
}

F32 NavSensor::readSensorTemp() {
    // Simulate sensor die temperature (deg C)
    return 25.0f + 3.0f * std::sin(static_cast<F32>(m_tickCount) * 0.005f);
}

}  // namespace MyFSW
