// ======================================================================
// \title  TcDeframerTester.hpp
// \author thomas-bc
// \brief  hpp file for TcDeframer component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_TcDeframerTester_HPP
#define Svc_Ccsds_TcDeframerTester_HPP

#include "Svc/Ccsds/TcDeframer/TcDeframer.hpp"
#include "Svc/Ccsds/TcDeframer/TcDeframerGTestBase.hpp"
#include "Svc/Ccsds/TestUtils/TestUtils.hpp"

namespace Svc {

namespace Ccsds {

class TcDeframerTester final : public TcDeframerGTestBase {
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

    //! Construct object TcDeframerTester
    TcDeframerTester();

    //! Destroy object TcDeframerTester
    ~TcDeframerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testDataReturn();
    void testNominalDeframing();
    void testInvalidScId();
    void testInvalidVcId();
    void testInvalidLengthToken();
    void testInvalidCrc();

    // Segment Header mode (CCSDS 232.0-B-4 4.1.3.2.2)
    void testFeatureOffIdentity();
    void testShModeUnsegmented();
    void testShModeAllFlags();
    void testShModeMinLength();
    void testShModeMissingSh();
    void testShModeTypeBc();
    void testShModeTypeBcOff();
    void testShModeOrderOfChecks();
    void testConfigureIdempotent();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Sets the component state to specific values, helpful for testing
    void setComponentState(U16 scid = 0, U8 vcid = 0, U8 seqNumber = 0, bool acceptAllVcid = true);

    Fw::Buffer assembleFrameBuffer(U8* data, U8 dataLength, U16 scid = 0, U8 vcid = 0, U8 seqNumber = 0);

    //! Build a TC frame into m_frameData with CcsdsTestUtils::buildTcFrame
    //! \param bufferSize when larger than the frame, the returned Fw::Buffer is grown to this size (zero-padded)
    Fw::Buffer buildFrame(const CcsdsTestUtils::TcFrameFields& fields,
                          const Fw::Buffer& payload,
                          FwSizeType bufferSize = 0);

    //! Fill a payload array with a deterministic pattern
    static void fillPayload(U8* payload, FwSizeType length);

    //! Send a frame and assert it was forwarded on dataOut with the expected payload and context, and no event
    void assertForwarded(Fw::Buffer& frame,
                         const U8* expectedPayload,
                         FwSizeType expectedLength,
                         U8 expectedVcId,
                         bool expectedShPresent,
                         U8 expectedSh);

    //! Send a frame and assert it was dropped: returned on dataReturnOut, no dataOut, no errorNotify
    void assertDropped(Fw::Buffer& frame);

    //! Send a frame and assert it was dropped: returned on dataReturnOut, no dataOut, errorNotify(expectedError)
    void assertDroppedWithError(Fw::Buffer& frame, FrameError expectedError);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    TcDeframer component;

    U8 m_frameData[300];  // data buffer used to produce test frames

    static const FwSizeType MAX_PAYLOAD_LENGTH = 200;  // payload bytes used by the tests
    U8 m_payload[MAX_PAYLOAD_LENGTH];                  // payload storage used to produce test frames
};

}  // namespace Ccsds

}  // namespace Svc

#endif
