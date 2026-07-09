// ======================================================================
// \title  ApidStripperTester.hpp
// \brief  hpp file for ApidStripper component test harness implementation class
// ======================================================================

#ifndef Svc_ApidStripperTester_HPP
#define Svc_ApidStripperTester_HPP

#include "Svc/ApidStripper/ApidStripper.hpp"
#include "Svc/ApidStripper/ApidStripperGTestBase.hpp"

namespace Svc {

class ApidStripperTester final : public ApidStripperGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object ApidStripperTester
    ApidStripperTester();

    //! Destroy object ApidStripperTester
    ~ApidStripperTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test nominal APID stripping
    void testNominalStrip();

    //! Test a buffer with an out-of-range APID (maps to INVALID_UNINITIALIZED)
    void testOutOfRangeApid();

    //! Test a buffer too small to contain an APID
    void testBufferTooSmall();

    //! Test pass-through of buffers returned on dataReturnIn
    void testDataReturn();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    ApidStripper component;
};

}  // namespace Svc

#endif
