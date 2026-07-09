// ======================================================================
// \title  ApidPrependerTestMain.cpp
// \brief  cpp file for ApidPrepender component test main function
// ======================================================================

#include "ApidPrependerTester.hpp"

#include <Fw/Test/UnitTest.hpp>

TEST(ApidPrepender, TestNominalPrepend) {
    COMMENT("Prepend the APID to a buffer");
    Svc::ApidPrependerTester tester;
    tester.testNominalPrepend();
}

TEST(ApidPrepender, TestAllocationFailure) {
    COMMENT("Handle a failed buffer allocation");
    Svc::ApidPrependerTester tester;
    tester.testAllocationFailure();
}

TEST(ApidPrepender, TestDataOutReturn) {
    COMMENT("Deallocate a returned buffer");
    Svc::ApidPrependerTester tester;
    tester.testDataOutReturn();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
