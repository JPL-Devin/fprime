// ======================================================================
// \title  UslpFramerTester.hpp
// \author Devin
// \brief  hpp file for UslpFramer component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_UslpFramerTester_HPP
#define Svc_Ccsds_UslpFramerTester_HPP

#include "Svc/Ccsds/UslpFramer/UslpFramer.hpp"
#include "Svc/Ccsds/UslpFramer/UslpFramerGTestBase.hpp"

namespace Svc {

namespace Ccsds {

class UslpFramerTester final : public UslpFramerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Maximum payload that fits in the fixed-size frame
    static constexpr FwSizeType PAYLOAD_CAPACITY = UslpFramer::UslpPayloadCapacity;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object UslpFramerTester
    UslpFramerTester();

    //! Destroy object UslpFramerTester
    ~UslpFramerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testComStatusPassthrough();
    void testNominalFraming();
    void testVcfCountIncrementAndWrap();
    void testIdleFillGaps();
    void testConfigureInvalidArgs();
    void testInputBufferTooLarge();
    void testDataReturn();
    void testBufferOwnershipState();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Build the byte-exact expected frame into m_expectedFrame
    void buildExpectedFrame(const U8* payload, FwSizeType payloadSize, U8 vcId, U8 mapId, U32 vcfCount);

    //! Frame a payload of the given size and verify the output frame byte-exactly,
    //! including the FECF check (frame parses back with a valid CRC16)
    void frameAndVerify(FwSizeType payloadSize, U8 vcId, U8 mapId, U32 expectedVcfCount);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    UslpFramer component;

    //! Storage for the hand-built expected frame
    U8 m_expectedFrame[ComCfg::UslpFrameFixedSize];

    //! Storage for payload data
    U8 m_payload[ComCfg::UslpFrameFixedSize];

    //! Number of frames sent so far in the current test (index into dataOut history)
    U32 m_framesSent;
};

}  // namespace Ccsds

}  // namespace Svc

#endif
