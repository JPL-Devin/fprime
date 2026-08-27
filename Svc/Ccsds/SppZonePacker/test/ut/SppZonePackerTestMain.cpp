// ======================================================================
// \title  SppZonePackerTestMain.cpp
// \author devin
// \brief  cpp file for SppZonePacker component test main function
// ======================================================================

#include "SppZonePackerTester.hpp"

namespace Svc {
namespace Ccsds {

TEST(SppZonePacker, MultiPacketsOneZone) {
    SppZonePackerTester tester;
    tester.testMultiPacketsOneZone();
}

TEST(SppZonePacker, PacketSpanning) {
    SppZonePackerTester tester;
    tester.testPacketSpanning();
}

TEST(SppZonePacker, ContinuationTailThenFresh) {
    SppZonePackerTester tester;
    tester.testContinuationTailThenFresh();
}

TEST(SppZonePacker, SendNowFlush) {
    SppZonePackerTester tester;
    tester.testSendNowFlush();
}

TEST(SppZonePacker, SchedulerFlush) {
    SppZonePackerTester tester;
    tester.testSchedulerFlush();
}

TEST(SppZonePacker, IdleStriping) {
    SppZonePackerTester tester;
    tester.testIdleStriping();
}

TEST(SppZonePacker, ExactFill) {
    SppZonePackerTester tester;
    tester.testExactFill();
}

TEST(SppZonePacker, ComStatusPassthrough) {
    SppZonePackerTester tester;
    tester.testComStatusPassthrough();
}

TEST(SppZonePacker, FailureRecovery) {
    SppZonePackerTester tester;
    tester.testFailureRecovery();
}

TEST(SppZonePacker, FlushWhileInFlight) {
    SppZonePackerTester tester;
    tester.testFlushWhileInFlight();
}

}  // namespace Ccsds
}  // namespace Svc

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
