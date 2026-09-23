// ----------------------------------------------------------------------
// CfdpReceiverTestMain.cpp
// ----------------------------------------------------------------------

#include "CfdpReceiverTester.hpp"

TEST(CfdpReceiver, ReceiveFile) {
    Svc::CfdpReceiverTester tester;
    tester.testReceiveFile();
}

TEST(CfdpReceiver, PduTooSmall) {
    Svc::CfdpReceiverTester tester;
    tester.testPduTooSmall();
}

TEST(CfdpReceiver, InvalidVersion) {
    Svc::CfdpReceiverTester tester;
    tester.testInvalidVersion();
}

TEST(CfdpReceiver, FileDataBeforeMetadata) {
    Svc::CfdpReceiverTester tester;
    tester.testFileDataBeforeMetadata();
}

TEST(CfdpReceiver, EofBeforeMetadata) {
    Svc::CfdpReceiverTester tester;
    tester.testEofBeforeMetadata();
}

TEST(CfdpReceiver, ChecksumFailure) {
    Svc::CfdpReceiverTester tester;
    tester.testChecksumFailure();
}

TEST(CfdpReceiver, CancelCommand) {
    Svc::CfdpReceiverTester tester;
    tester.testCancelCommand();
}

TEST(CfdpReceiver, CancelEof) {
    Svc::CfdpReceiverTester tester;
    tester.testCancelEof();
}

TEST(CfdpReceiver, PingResponse) {
    Svc::CfdpReceiverTester tester;
    tester.testPingResponse();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
