// ======================================================================
// \title  SdlsSaRouterTestMain.cpp
// \author lestarch-autobot
// \brief  cpp file for SdlsSaRouter component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "STest/Scenario/BoundedScenario.hpp"
#include "STest/Scenario/RandomScenario.hpp"
#include "Svc/Ccsds/SdlsSaRouter/test/ut/SdlsSaRouterTester.hpp"

using Svc::Ccsds::SdlsSaRouterTester;

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

// Verify decryptIn routes known SAs to the mapped port and passes the
// downstream status through to the caller.
TEST(SdlsSaRouter, RouteKnownSa) {
    SdlsSaRouterTester tester;
    SdlsSaRouterTester::Route__KnownSa rule;
    rule.apply(tester);
}

// Verify decryptIn returns UNKNOWN_SA for an unmapped SA and UNKNOWN_PORT
// for an SA mapped to an out-of-range port, without forwarding.
TEST(SdlsSaRouter, RouteErrors) {
    SdlsSaRouterTester tester;
    SdlsSaRouterTester::Route__UnknownSa ruleUnknownSa;
    SdlsSaRouterTester::Route__UnknownPort ruleUnknownPort;
    ruleUnknownSa.apply(tester);
    ruleUnknownPort.apply(tester);
}

// Verify decrypted data flows upstream (saDecryptIn -> decryptOut) and its
// ownership return is routed back to the originating port
// (decryptReturnIn -> saDecryptReturnOut).
TEST(SdlsSaRouter, DecryptDataAndReturn) {
    SdlsSaRouterTester tester;
    SdlsSaRouterTester::DataFlow__DecryptData ruleData;
    SdlsSaRouterTester::DataFlow__DecryptReturn ruleReturn;
    ruleData.apply(tester);
    ruleReturn.apply(tester);
}

// Verify incoming iv/data buffers are passed upstream for deallocation
// (saBufferReturnIn -> bufferReturnOut).
TEST(SdlsSaRouter, BufferReturn) {
    SdlsSaRouterTester tester;
    SdlsSaRouterTester::DataFlow__BufferReturn rule;
    rule.apply(tester);
}

// Randomized test: apply rules in a random sequence for a large number of iterations
TEST(SdlsSaRouter, RandomizedTesting) {
    const U32 numRulesToApply = 10000;
    SdlsSaRouterTester tester;
    SdlsSaRouterTester::Route__KnownSa ruleKnownSa;
    SdlsSaRouterTester::Route__UnknownSa ruleUnknownSa;
    SdlsSaRouterTester::Route__UnknownPort ruleUnknownPort;
    SdlsSaRouterTester::DataFlow__DecryptData ruleData;
    SdlsSaRouterTester::DataFlow__DecryptReturn ruleReturn;
    SdlsSaRouterTester::DataFlow__BufferReturn ruleBufferReturn;

    STest::Rule<SdlsSaRouterTester>* rules[] = {
        &ruleKnownSa, &ruleUnknownSa, &ruleUnknownPort, &ruleData, &ruleReturn, &ruleBufferReturn,
    };

    STest::RandomScenario<SdlsSaRouterTester> random("Random Rules", rules, FW_NUM_ARRAY_ELEMENTS(rules));
    STest::BoundedScenario<SdlsSaRouterTester> bounded("Bounded Random Rules Scenario", random, numRulesToApply);
    const U32 numSteps = bounded.run(tester);
    printf("Ran %u steps.\n", numSteps);
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
