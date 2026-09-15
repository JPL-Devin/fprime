// ======================================================================
// \title  NavSensor.hpp
// \brief  hpp file for NavSensor component implementation class
// ======================================================================

#ifndef MyFSW_NavSensor_HPP
#define MyFSW_NavSensor_HPP

#include "MyFSW/NavSensor/NavSensorComponentAc.hpp"

namespace MyFSW {

class NavSensor final : public NavSensorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct a NavSensor
    NavSensor(const char* compName);

    //! Destroy a NavSensor
    ~NavSensor();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler for SET_NAV_MODE command
    void SET_NAV_MODE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        MyFSW::NavMode mode
    ) final;

    //! Handler for ZERO_GYRO_BIAS command
    void ZERO_GYRO_BIAS_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq
    ) final;

    //! Handler for SELF_TEST command
    void SELF_TEST_cmdHandler(
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

    //! Simulate gyroscope reading
    Vector3 readGyroscope();

    //! Simulate accelerometer reading
    Vector3 readAccelerometer();

    //! Simulate magnetometer reading
    Vector3 readMagnetometer();

    //! Simulate sensor temperature reading
    F32 readSensorTemp();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Current operating mode
    NavMode m_mode;

    //! Gyroscope bias values (deg/s)
    F32 m_gyroBias[3];

    //! Total sample count
    U32 m_sampleCount;

    //! Tick counter for simulation
    U32 m_tickCount;
};

}  // namespace MyFSW

#endif
