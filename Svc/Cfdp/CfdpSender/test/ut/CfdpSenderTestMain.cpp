// ----------------------------------------------------------------------
// CfdpSenderTestMain.cpp
// ----------------------------------------------------------------------

#include "CfdpSenderTester.hpp"

// ----------------------------------------------------------------------
// Sender state machine tests
// ----------------------------------------------------------------------

TEST(CfdpSender, SendFileCommand) {
    Svc::CfdpSenderTester tester;
    tester.testSendFileCommand();
}

TEST(CfdpSender, SendFilePort) {
    Svc::CfdpSenderTester tester;
    tester.testSendFilePort();
}

TEST(CfdpSender, SendPartialFile) {
    Svc::CfdpSenderTester tester;
    tester.testSendPartialFile();
}

TEST(CfdpSender, CancelDuringTransfer) {
    Svc::CfdpSenderTester tester;
    tester.testCancelDuringTransfer();
}

TEST(CfdpSender, SendWhileBusy) {
    Svc::CfdpSenderTester tester;
    tester.testSendWhileBusy();
}

TEST(CfdpSender, FileOpenError) {
    Svc::CfdpSenderTester tester;
    tester.testFileOpenError();
}

TEST(CfdpSender, TransmitPduFailureRetry) {
    Svc::CfdpSenderTester tester;
    tester.testTransmitPduFailureRetry();
}

TEST(CfdpSender, PingResponse) {
    Svc::CfdpSenderTester tester;
    tester.testPingResponse();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
