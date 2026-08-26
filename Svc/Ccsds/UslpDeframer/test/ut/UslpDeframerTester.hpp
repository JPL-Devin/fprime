// ======================================================================
// \title  UslpDeframerTester.hpp
// \author thomas-bc
// \brief  hpp file for UslpDeframer component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_UslpDeframerTester_HPP
#define Svc_Ccsds_UslpDeframerTester_HPP

#include "Svc/Ccsds/UslpDeframer/UslpDeframer.hpp"
#include "Svc/Ccsds/UslpDeframer/UslpDeframerGTestBase.hpp"

namespace Svc {

namespace Ccsds {

class UslpDeframerTester final : public UslpDeframerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Parameters describing a USLP test frame to assemble
    struct FrameParams {
        U8 tfvn = 0xC;            //!< Transfer Frame Version Number
        U16 scid = 0x0044;        //!< Spacecraft ID
        U8 sourceOrDest = 1;      //!< Source-or-Destination Identifier (1 = destination)
        U8 vcid = 0;              //!< Virtual Channel ID
        U8 mapId = 0;             //!< MAP ID
        U8 eofph = 0;             //!< End of Frame Primary Header flag
        U8 bypass = 0;            //!< Bypass/Sequence Control flag
        U8 protCmd = 0;           //!< Protocol Control Command flag
        U8 spares = 0;            //!< Reserved spares
        U8 ocf = 0;               //!< OCF flag
        U8 vcfCountLength = 0;    //!< VCF Count Length in octets
        U8 rule = 0x7;            //!< TFDF construction rule
        U8 upid = 0;              //!< TFDF UPID
        bool corruptCrc = false;  //!< Corrupt the FECF value
        I32 lengthDelta = 0;      //!< Delta applied to the frame length field
    };

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object UslpDeframerTester
    UslpDeframerTester();

    //! Destroy object UslpDeframerTester
    ~UslpDeframerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testDataReturn();
    void testNominalDeframing();
    void testNominalVcfCountLengths();
    void testShortFrame();
    void testInvalidVersion();
    void testTruncatedFrame();
    void testInvalidScId();
    void testInvalidSourceOrDest();
    void testInvalidLength();
    void testLengthWrap();
    void testInvalidVcId();
    void testAcceptAllVcid();
    void testInvalidMapId();
    void testProtocolCommand();
    void testInvalidSpares();
    void testOcfFlag();
    void testInvalidVcfCountLength();
    void testStructuralLengthUnderflow();
    void testInvalidCrc();
    void testInvalidTfdfRule();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Assemble a USLP frame into m_frameData and return a buffer over it
    Fw::Buffer assembleFrameBuffer(const U8* payload, FwSizeType payloadLength, const FrameParams& params);

    //! Invoke dataIn with the given buffer and assert the frame was rejected with the given error
    void assertReject(Fw::Buffer& buffer, Svc::Ccsds::FrameError expectedError);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    UslpDeframer component;

    U8 m_frameData[300];  //!< data buffer used to produce test frames
};

}  // namespace Ccsds

}  // namespace Svc

#endif
