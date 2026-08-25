// ======================================================================
// \title  UslpDeframerTestMain.cpp
// \author thomas-bc
// \brief  cpp file for UslpDeframer component test main function
// ======================================================================

#include "UslpDeframerTester.hpp"

TEST(UslpDeframer, testDataReturn) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testDataReturn();
}

TEST(UslpDeframer, testNominalDeframing) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testNominalDeframing();
}

TEST(UslpDeframer, testNominalVcfCountLengths) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testNominalVcfCountLengths();
}

TEST(UslpDeframer, testShortFrame) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testShortFrame();
}

TEST(UslpDeframer, testInvalidVersion) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidVersion();
}

TEST(UslpDeframer, testTruncatedFrame) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testTruncatedFrame();
}

TEST(UslpDeframer, testInvalidScId) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidScId();
}

TEST(UslpDeframer, testInvalidSourceOrDest) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidSourceOrDest();
}

TEST(UslpDeframer, testInvalidLength) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidLength();
}

TEST(UslpDeframer, testLengthWrap) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testLengthWrap();
}

TEST(UslpDeframer, testInvalidVcId) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidVcId();
}

TEST(UslpDeframer, testAcceptAllVcid) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testAcceptAllVcid();
}

TEST(UslpDeframer, testInvalidMapId) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidMapId();
}

TEST(UslpDeframer, testProtocolCommand) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testProtocolCommand();
}

TEST(UslpDeframer, testInvalidSpares) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidSpares();
}

TEST(UslpDeframer, testOcfFlag) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testOcfFlag();
}

TEST(UslpDeframer, testInvalidVcfCountLength) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidVcfCountLength();
}

TEST(UslpDeframer, testInvalidCrc) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidCrc();
}

TEST(UslpDeframer, testInvalidTfdfRule) {
    Svc::Ccsds::UslpDeframerTester tester;
    tester.testInvalidTfdfRule();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
