// ======================================================================
// \title  SdlsSaRouterTester.cpp
// \author lestarch-autobot
// \brief  cpp file for SdlsSaRouter component test harness implementation class
// ======================================================================

#include "Svc/Ccsds/SdlsSaRouter/test/ut/SdlsSaRouterTester.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SdlsSaRouterTester ::SdlsSaRouterTester()
    : SdlsSaRouterGTestBase("SdlsSaRouterTester", SdlsSaRouterTester::MAX_HISTORY_SIZE), component("SdlsSaRouter") {
    this->initComponents();
    this->connectPorts();

    // Non-linear, sparse SA-to-port test map with one out-of-range entry
    this->m_validSas[0] = 10;
    this->m_validPorts[0] = 0;
    this->m_validSas[1] = 5;
    this->m_validPorts[1] = 1;
    this->m_validSas[2] = 42;
    this->m_validPorts[2] = 2;

    Svc::Ccsds::SaMap saMap;
    for (FwSizeType i = 0; i < VALID_SA_COUNT; i++) {
        saMap[i] = Svc::Ccsds::SaMapEntry(this->m_validSas[i], this->m_validPorts[i]);
    }
    saMap[VALID_SA_COUNT] = Svc::Ccsds::SaMapEntry(OUT_OF_RANGE_SA, SdlsCfg::SaRouterPortCount);
    this->component.configure(saMap);
}

SdlsSaRouterTester ::~SdlsSaRouterTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Handler overrides for typed from ports
// ----------------------------------------------------------------------

Svc::Ccsds::SdlsStatus SdlsSaRouterTester ::from_saDecryptOut_handler(FwIndexType portNum,
                                                                      U16 securityAssociationIndex,
                                                                      Fw::Buffer& data,
                                                                      const ComCfg::FrameContext& context) {
    this->m_lastSaDecryptOutPort = portNum;
    this->pushFromPortEntry_saDecryptOut(securityAssociationIndex, data, context);
    return this->m_downstreamStatus;
}

void SdlsSaRouterTester ::from_saDecryptReturnOut_handler(FwIndexType portNum,
                                                          Fw::Buffer& data,
                                                          const ComCfg::FrameContext& context) {
    this->m_lastSaDecryptReturnOutPort = portNum;
    this->pushFromPortEntry_saDecryptReturnOut(data, context);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

U8* SdlsSaRouterTester ::getFreePoolBuffer() {
    for (FwSizeType i = 0; i < SdlsCfg::SaRouterMaxOutstandingBuffers; i++) {
        if (this->shadow.shadow_outstanding.count(this->m_pool[i]) == 0) {
            return this->m_pool[i];
        }
    }
    return nullptr;
}

}  // namespace Ccsds

}  // namespace Svc
