// ======================================================================
// \title  TcDeframerTestMain.cpp
// \author thomas-bc
// \brief  cpp file for TcDeframer component test main function
// ======================================================================

#include "TcDeframerTester.hpp"

TEST(TcDeframer, testDataReturn) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testDataReturn();
}

TEST(TcDeframer, testNominalDeframing) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testNominalDeframing();
}

TEST(TcDeframer, testInvalidScId) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testInvalidScId();
}

TEST(TcDeframer, testInvalidVcId) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testInvalidVcId();
}

TEST(TcDeframer, testInvalidLengthToken) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testInvalidLengthToken();
}

TEST(TcDeframer, testInvalidCrc) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testInvalidCrc();
}

TEST(TcDeframer, testFeatureOffIdentity) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testFeatureOffIdentity();
}

TEST(TcDeframer, testShModeUnsegmented) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeUnsegmented();
}

TEST(TcDeframer, testShModeAllFlags) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeAllFlags();
}

TEST(TcDeframer, testShModeMinLength) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeMinLength();
}

TEST(TcDeframer, testShModeMissingSh) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeMissingSh();
}

TEST(TcDeframer, testShModeTypeBc) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeTypeBc();
}

TEST(TcDeframer, testShModeTypeBcOff) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeTypeBcOff();
}

TEST(TcDeframer, testShModeOrderOfChecks) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testShModeOrderOfChecks();
}

TEST(TcDeframer, testConfigureIdempotent) {
    Svc::Ccsds::TcDeframerTester tester;
    tester.testConfigureIdempotent();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
