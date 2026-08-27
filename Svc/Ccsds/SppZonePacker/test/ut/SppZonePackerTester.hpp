// ======================================================================
// \title  SppZonePackerTester.hpp
// \author devin
// \brief  hpp file for SppZonePacker component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_SppZonePackerTester_HPP
#define Svc_Ccsds_SppZonePackerTester_HPP

#include "Svc/Ccsds/SppZonePacker/SppZonePacker.hpp"
#include "Svc/Ccsds/SppZonePacker/SppZonePackerGTestBase.hpp"

namespace Svc {

namespace Ccsds {

class SppZonePackerTester final : public SppZonePackerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 20;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Zone geometry used throughout the tests
    static const FwSizeType TEST_ZONE_SIZE = 100;
    static const FwSizeType TEST_HEADROOM = 8;
    static const FwSizeType TEST_TRAILER = 2;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object SppZonePackerTester
    SppZonePackerTester();

    //! Destroy object SppZonePackerTester
    ~SppZonePackerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testMultiPacketsOneZone();
    void testPacketSpanning();
    void testContinuationTailThenFresh();
    void testSendNowFlush();
    void testSchedulerFlush();
    void testIdleStriping();
    void testExactFill();
    void testComStatusPassthrough();
    void testFailureRecovery();
    void testFlushWhileInFlight();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Send a packet of the given size filled with an incrementing pattern
    void sendPacket(Fw::Buffer& packet, bool sendNow = false);

    //! Return the most recently emitted zone buffer back to the component
    void returnZone(FwIndexType historyIndex);

    //! Get the emitted zone at the given dataOut history index
    Fw::Buffer getZone(FwIndexType historyIndex);

    //! Get the context of the emitted zone at the given dataOut history index
    ComCfg::FrameContext getZoneContext(FwIndexType historyIndex);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    SppZonePacker component;
};

}  // namespace Ccsds

}  // namespace Svc

#endif
