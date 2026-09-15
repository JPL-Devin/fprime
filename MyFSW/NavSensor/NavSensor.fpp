module MyFSW {

  @ 3-axis vector for sensor readings
  struct Vector3 {
    x: F32 format "{.4f}"
    y: F32 format "{.4f}"
    z: F32 format "{.4f}"
  }

  @ Navigation sensor operating mode
  enum NavMode {
    IDLE        @< Sensor is idle, not sampling
    NOMINAL     @< Normal sampling rate
    HIGH_RATE   @< High-rate sampling for maneuvers
  }

  @ Queued component that reads navigation/IMU sensor data and
  @ provides attitude and rate telemetry
  queued component NavSensor {

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

    @ Set the navigation sensor operating mode
    async command SET_NAV_MODE(
      mode: MyFSW.NavMode @< Desired operating mode
    ) opcode 0x00

    @ Zero the gyroscope bias
    async command ZERO_GYRO_BIAS opcode 0x01

    @ Perform a sensor self-test
    async command SELF_TEST opcode 0x02

    # ------------------------------------------------------------------
    # Telemetry
    # ------------------------------------------------------------------

    @ Current angular rates from gyroscope (deg/s)
    telemetry GYRO_RATES: MyFSW.Vector3 id 0

    @ Current accelerometer readings (m/s^2)
    telemetry ACCEL_DATA: MyFSW.Vector3 id 1

    @ Current magnetometer readings (uT)
    telemetry MAG_DATA: MyFSW.Vector3 id 2

    @ Current operating mode
    telemetry NAV_MODE: MyFSW.NavMode id 3

    @ Total number of sensor samples taken
    telemetry SAMPLE_COUNT: U32 id 4

    @ Sensor temperature (deg C)
    telemetry SENSOR_TEMP: F32 id 5 format "{.1f}"

    # ------------------------------------------------------------------
    # Events
    # ------------------------------------------------------------------

    @ Navigation mode changed
    event NAV_MODE_CHANGED(
      oldMode: MyFSW.NavMode @< Previous mode
      newMode: MyFSW.NavMode @< New mode
    ) severity activity high \
      id 0 \
      format "Nav mode changed from {} to {}"

    @ Gyroscope bias zeroed
    event GYRO_BIAS_ZEROED(
      biasX: F32 @< X-axis bias removed
      biasY: F32 @< Y-axis bias removed
      biasZ: F32 @< Z-axis bias removed
    ) severity activity low \
      id 1 \
      format "Gyro bias zeroed: [{.4f}, {.4f}, {.4f}] deg/s"

    @ Self-test completed
    event SELF_TEST_PASSED \
      severity activity high \
      id 2 \
      format "NavSensor self-test PASSED"

    @ Self-test failed
    event SELF_TEST_FAILED(
      errorCode: U32 @< Error code from self-test
    ) severity warning high \
      id 3 \
      format "NavSensor self-test FAILED with error code {}"

    @ Sensor data rate anomaly
    event HIGH_NOISE_DETECTED(
      axis: string size 8 @< Axis name
      rmsNoise: F32        @< RMS noise level
    ) severity warning low \
      id 4 \
      format "High noise on {} axis: RMS = {.4f}"

    # ------------------------------------------------------------------
    # Interfaces
    # ------------------------------------------------------------------
    import Fw.Event
    import Fw.Command
    import Fw.Channel

  }

}
