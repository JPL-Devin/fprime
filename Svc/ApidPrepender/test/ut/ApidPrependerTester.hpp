// ======================================================================
// \title  ApidPrependerTester.hpp
// \brief  hpp file for ApidPrepender component test harness implementation class
// ======================================================================

#ifndef Svc_ApidPrependerTester_HPP
#define Svc_ApidPrependerTester_HPP

#include "Svc/ApidPrepender/ApidPrepender.hpp"
#include "Svc/ApidPrepender/ApidPrependerGTestBase.hpp"

namespace Svc {

class ApidPrependerTester final : public ApidPrependerGTestBase {
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

    //! Construct object ApidPrependerTester
    ApidPrependerTester();

    //! Destroy object ApidPrependerTester
    ~ApidPrependerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test nominal APID prepending
    void testNominalPrepend();

    //! Test behavior when buffer allocation fails
    void testAllocationFailure();

    //! Test deallocation of buffers returned on dataOutReturn
    void testDataOutReturn();

  private:
    // ----------------------------------------------------------------------
    // Test Harness: Handler implementations for output ports
    // ----------------------------------------------------------------------

    Fw::Buffer from_allocate_handler(FwIndexType portNum, FwSizeType size) override;

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

    U8 m_buffer_slot[2048];
    Fw::Buffer m_buffer;            //!< buffer to be returned by mocked allocate call
    bool m_failAllocation = false;  //!< when set, from_allocate_handler returns an empty buffer

    //! The component under test
    ApidPrepender component;
};

}  // namespace Svc

#endif
