// ======================================================================
// \title  ComDataBufferAdapterTestMain.cpp
// \brief  cpp file for ComDataBufferAdapter component test main function
// ======================================================================

#include "ComDataBufferAdapterTester.hpp"

#include <Fw/Test/UnitTest.hpp>
#include <STest/Random/Random.hpp>

TEST(ComDataBufferAdapter, SendDefaultContext) {
    COMMENT("bufferIn is forwarded on dataOut with the default context");
    REQUIREMENT("SVC-COM-DATA-BUFFER-ADAPTER-001");
    Svc::ComDataBufferAdapterTester tester;
    tester.testSendDefaultContext();
}

TEST(ComDataBufferAdapter, SendConfiguredContext) {
    COMMENT("bufferIn is forwarded on dataOut with the configured context");
    REQUIREMENT("SVC-COM-DATA-BUFFER-ADAPTER-001");
    REQUIREMENT("SVC-COM-DATA-BUFFER-ADAPTER-005");
    Svc::ComDataBufferAdapterTester tester;
    tester.testSendConfiguredContext();
}

TEST(ComDataBufferAdapter, SendReturn) {
    COMMENT("dataReturnIn is forwarded on bufferInReturn");
    REQUIREMENT("SVC-COM-DATA-BUFFER-ADAPTER-002");
    Svc::ComDataBufferAdapterTester tester;
    tester.testSendReturn();
}

TEST(ComDataBufferAdapter, Receive) {
    COMMENT("dataIn is forwarded on bufferOut");
    REQUIREMENT("SVC-COM-DATA-BUFFER-ADAPTER-003");
    Svc::ComDataBufferAdapterTester tester;
    tester.testReceive();
}

TEST(ComDataBufferAdapter, ReceiveReturn) {
    COMMENT("bufferOutReturn is forwarded on dataReturnOut");
    REQUIREMENT("SVC-COM-DATA-BUFFER-ADAPTER-004");
    Svc::ComDataBufferAdapterTester tester;
    tester.testReceiveReturn();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
