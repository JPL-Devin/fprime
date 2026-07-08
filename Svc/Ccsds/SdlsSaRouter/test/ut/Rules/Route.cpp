// ======================================================================
// \title  Route.cpp
// \author lestarch-autobot
// \brief  Rule implementations for the Route rule group
//
// These rules exercise the decryptIn port: SA-to-port routing, status
// pass-through, and the UNKNOWN_SA / UNKNOWN_PORT error returns.
// ======================================================================

#include "STest/Pick/Pick.hpp"
#include "Svc/Ccsds/SdlsSaRouter/test/ut/SdlsSaRouterTester.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Route.KnownSa
// ----------------------------------------------------------------------

bool SdlsSaRouterTester::Route__KnownSa__precondition() const {
    return this->m_decryptOutConnected;
}

void SdlsSaRouterTester::Route__KnownSa__action() {
    this->clearHistory();

    const FwSizeType pick = this->pickMapEntry();
    const U16 sa = this->m_mapSas[pick];
    const FwIndexType expectedPort = this->m_mapPorts[pick];

    // Stage a random downstream status to verify pass-through
    this->m_downstreamStatus = (STest::Pick::lowerUpper(0, 1) == 0) ? Svc::Ccsds::SdlsStatus::SUCCESS
                                                                    : Svc::Ccsds::SdlsStatus::DECRYPTION_FAILURE;
    U8 storage[TEST_BUFFER_SIZE];
    Fw::Buffer buffer(storage, sizeof storage);
    ComCfg::FrameContext context;

    const Svc::Ccsds::SdlsStatus status = this->invoke_to_decryptIn(0, sa, buffer, context);

    ASSERT_EQ(status, this->m_downstreamStatus);
    ASSERT_from_saDecryptOut_SIZE(1);
    ASSERT_from_saDecryptOut(0, sa, buffer, context);
    ASSERT_EQ(this->m_lastSaDecryptOutPort, expectedPort);
}

// ----------------------------------------------------------------------
// Route.UnknownSa
// ----------------------------------------------------------------------

bool SdlsSaRouterTester::Route__UnknownSa__precondition() const {
    return true;
}

void SdlsSaRouterTester::Route__UnknownSa__action() {
    this->clearHistory();

    // Pick an SA outside the configured map
    U16 sa = 0;
    do {
        sa = static_cast<U16>(STest::Pick::lowerUpper(0, 0xFFFF));
    } while (this->isMappedSa(sa));
    U8 storage[TEST_BUFFER_SIZE];
    Fw::Buffer buffer(storage, sizeof storage);
    ComCfg::FrameContext context;

    const Svc::Ccsds::SdlsStatus status = this->invoke_to_decryptIn(0, sa, buffer, context);

    ASSERT_EQ(status, Svc::Ccsds::SdlsStatus::UNKNOWN_SA);
    ASSERT_from_saDecryptOut_SIZE(0);
}

// ----------------------------------------------------------------------
// Route.UnknownPort
// ----------------------------------------------------------------------

bool SdlsSaRouterTester::Route__UnknownPort__precondition() const {
    // Requires the saDecryptOut ports to be left unconnected
    return !this->m_decryptOutConnected;
}

void SdlsSaRouterTester::Route__UnknownPort__action() {
    this->clearHistory();

    // Any mapped SA routes to an unconnected port in this configuration
    const U16 sa = this->m_mapSas[this->pickMapEntry()];
    U8 storage[TEST_BUFFER_SIZE];
    Fw::Buffer buffer(storage, sizeof storage);
    ComCfg::FrameContext context;

    const Svc::Ccsds::SdlsStatus status = this->invoke_to_decryptIn(0, sa, buffer, context);

    ASSERT_EQ(status, Svc::Ccsds::SdlsStatus::UNKNOWN_PORT);
    ASSERT_from_saDecryptOut_SIZE(0);
}

}  // namespace Ccsds

}  // namespace Svc
