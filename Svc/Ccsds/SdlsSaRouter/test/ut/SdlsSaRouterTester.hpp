// ======================================================================
// \title  SdlsSaRouterTester.hpp
// \author lestarch-autobot
// \brief  hpp file for SdlsSaRouter component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_SdlsSaRouterTester_HPP
#define Svc_Ccsds_SdlsSaRouterTester_HPP

#include "Svc/Ccsds/SdlsSaRouter/SdlsSaRouter.hpp"
#include "Svc/Ccsds/SdlsSaRouter/SdlsSaRouterGTestBase.hpp"
#include "Svc/Ccsds/SdlsSaRouter/test/ut/TestState/TestState.hpp"
#include "TestUtils/RuleBasedTesting.hpp"

namespace Svc {

namespace Ccsds {

class SdlsSaRouterTester : public SdlsSaRouterGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    //! Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Size of each test buffer in the pool
    static const FwSizeType TEST_BUFFER_SIZE = 64;

    //! Number of valid SA map entries used for testing
    static const FwSizeType VALID_SA_COUNT = 3;

    //! SA mapped to an out-of-range port index
    static const U16 OUT_OF_RANGE_SA = 99;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object SdlsSaRouterTester
    SdlsSaRouterTester();

    //! Destroy object SdlsSaRouterTester
    ~SdlsSaRouterTester();

  private:
    // ----------------------------------------------------------------------
    // Handler overrides for typed from ports
    // ----------------------------------------------------------------------

    //! Override recording the invoked port number and returning the staged status
    Svc::Ccsds::SdlsStatus from_saDecryptOut_handler(FwIndexType portNum,
                                                     U16 securityAssociationIndex,
                                                     Fw::Buffer& data,
                                                     const ComCfg::FrameContext& context) override;

    //! Override recording the invoked port number
    void from_saDecryptReturnOut_handler(FwIndexType portNum,
                                         Fw::Buffer& data,
                                         const ComCfg::FrameContext& context) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Return a pool buffer pointer not currently outstanding, or nullptr if all are in use
    U8* getFreePoolBuffer();

  public:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    SdlsSaRouter component;

    //! Shadow state for rule-based testing
    SdlsSaRouterTestState shadow;

    //! Status returned by the downstream decryptor stub (from_saDecryptOut)
    Svc::Ccsds::SdlsStatus m_downstreamStatus = Svc::Ccsds::SdlsStatus::SUCCESS;

    //! Port number of the last from_saDecryptOut invocation
    FwIndexType m_lastSaDecryptOutPort = -1;

    //! Port number of the last from_saDecryptReturnOut invocation
    FwIndexType m_lastSaDecryptReturnOutPort = -1;

    //! Pool of buffers used as outstanding decrypted data
    U8 m_pool[SdlsCfg::SaRouterMaxOutstandingBuffers][TEST_BUFFER_SIZE];

    //! SAs configured in the test routing map, paired with their expected ports
    U16 m_validSas[VALID_SA_COUNT];

    //! Expected downstream port for each valid SA
    FwIndexType m_validPorts[VALID_SA_COUNT];

  public:
    // ----------------------------------------------------------------------
    // Rule Based Testing
    // ----------------------------------------------------------------------

    //! Rules for the decryptIn port (SA routing)
    FW_RBT_DEFINE_RULE(SdlsSaRouterTester, Route, KnownSa);
    FW_RBT_DEFINE_RULE(SdlsSaRouterTester, Route, UnknownSa);
    FW_RBT_DEFINE_RULE(SdlsSaRouterTester, Route, UnknownPort);

    //! Rules for the decrypted data and buffer return paths
    FW_RBT_DEFINE_RULE(SdlsSaRouterTester, DataFlow, DecryptData);
    FW_RBT_DEFINE_RULE(SdlsSaRouterTester, DataFlow, DecryptReturn);
    FW_RBT_DEFINE_RULE(SdlsSaRouterTester, DataFlow, BufferReturn);
};

}  // namespace Ccsds

}  // namespace Svc

#endif
