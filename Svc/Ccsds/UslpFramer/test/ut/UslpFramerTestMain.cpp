// ======================================================================
// \title  UslpFramerTestMain.cpp
// \author Devin
// \brief  cpp file for UslpFramer component test main function
// ======================================================================

#include "UslpFramerTester.hpp"

TEST(UslpFramer, testComStatusPassthrough) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testComStatusPassthrough();
}

TEST(UslpFramer, testNominalFraming) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testNominalFraming();
}

TEST(UslpFramer, testVcfCountIncrementAndWrap) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testVcfCountIncrementAndWrap();
}

TEST(UslpFramer, testIdleFillGaps) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testIdleFillGaps();
}

TEST(UslpFramer, testConfigureInvalidArgs) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testConfigureInvalidArgs();
}

TEST(UslpFramer, testInputBufferTooLarge) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testInputBufferTooLarge();
}

TEST(UslpFramer, testDataReturn) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testDataReturn();
}

TEST(UslpFramer, testBufferOwnershipState) {
    Svc::Ccsds::UslpFramerTester tester;
    tester.testBufferOwnershipState();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
