// ======================================================================
// \title  SdlsSaRouterTester.cpp
// \author lestarch-autobot
// \brief  cpp file for SdlsSaRouter component test harness implementation class
// ======================================================================

#include "Svc/Ccsds/SdlsSaRouter/test/ut/SdlsSaRouterTester.hpp"
#include "STest/Pick/Pick.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SdlsSaRouterTester ::SdlsSaRouterTester(bool connectDecryptOut)
    : SdlsSaRouterGTestBase("SdlsSaRouterTester", SdlsSaRouterTester::MAX_HISTORY_SIZE),
      component("SdlsSaRouter"),
      m_decryptOutConnected(connectDecryptOut) {
    this->initComponents();
    this->connectPortsCustom(connectDecryptOut);

    // Mirror the compile-time map for computing expected routing
    const SdlsCfg::SaMap saMap;
    for (SdlsCfg::SaMap::SizeType i = 0; i < SdlsCfg::SaMap::SIZE; i++) {
        this->m_mapSas[i] = saMap[i].get_securityAssociationIndex();
        this->m_mapPorts[i] = saMap[i].get_portIndex();
    }
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

void SdlsSaRouterTester ::connectPortsCustom(bool connectDecryptOut) {
    // Connect typed input ports
    this->connect_to_decryptIn(0, this->component.get_decryptIn_InputPort(0));
    this->connect_to_decryptReturnIn(0, this->component.get_decryptReturnIn_InputPort(0));
    for (FwIndexType i = 0; i < SdlsCfg::SaRouterPortCount; i++) {
        this->connect_to_saBufferReturnIn(i, this->component.get_saBufferReturnIn_InputPort(i));
        this->connect_to_saDecryptIn(i, this->component.get_saDecryptIn_InputPort(i));
    }

    // Connect typed output ports, optionally leaving saDecryptOut unconnected
    this->component.set_bufferReturnOut_OutputPort(0, this->get_from_bufferReturnOut(0));
    this->component.set_decryptOut_OutputPort(0, this->get_from_decryptOut(0));
    for (FwIndexType i = 0; i < SdlsCfg::SaRouterPortCount; i++) {
        if (connectDecryptOut) {
            this->component.set_saDecryptOut_OutputPort(i, this->get_from_saDecryptOut(i));
        }
        this->component.set_saDecryptReturnOut_OutputPort(i, this->get_from_saDecryptReturnOut(i));
    }
}

bool SdlsSaRouterTester ::isMappedSa(U16 sa) const {
    for (FwSizeType i = 0; i < SdlsCfg::SaRouterMapEntryCount; i++) {
        if (this->m_mapSas[i] == sa) {
            return true;
        }
    }
    return false;
}

FwSizeType SdlsSaRouterTester ::pickMapEntry() const {
    return STest::Pick::lowerUpper(0, static_cast<U32>(SdlsCfg::SaRouterMapEntryCount - 1));
}

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
