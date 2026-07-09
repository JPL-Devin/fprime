// ======================================================================
// \title  ApidStripperTestMain.cpp
// \brief  cpp file for ApidStripper component test main function
// ======================================================================

#include "ApidStripperTester.hpp"

#include <Fw/Test/UnitTest.hpp>

TEST(ApidStripper, TestNominalStrip) {
    COMMENT("Strip the APID from a buffer");
    Svc::ApidStripperTester tester;
    tester.testNominalStrip();
}

TEST(ApidStripper, TestOutOfRangeApid) {
    COMMENT("Handle an out-of-range APID");
    Svc::ApidStripperTester tester;
    tester.testOutOfRangeApid();
}

TEST(ApidStripper, TestBufferTooSmall) {
    COMMENT("Handle a buffer too small to contain an APID");
    Svc::ApidStripperTester tester;
    tester.testBufferTooSmall();
}

TEST(ApidStripper, TestDataReturn) {
    COMMENT("Pass through a returned buffer");
    Svc::ApidStripperTester tester;
    tester.testDataReturn();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
