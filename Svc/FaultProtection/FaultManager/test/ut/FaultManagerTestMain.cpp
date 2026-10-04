// ======================================================================
// \title  FaultManagerTestMain.cpp
// \author mstarch
// \brief  cpp file for FaultManager component test main function
// ======================================================================

#include "FaultManagerTester.hpp"
#include "Fw/Test/UnitTest.hpp"

using Svc::FaultProtection::FaultManagerTester;

TEST(Nominal, Idle) {
    COMMENT("Ticks without a fault report leave the FaultManager idle");
    FaultManagerTester tester;
    tester.testIdle();
}

TEST(Nominal, Response) {
    COMMENT("A report is latched, counted down, dispatched, completed, and the latch cleared");
    REQUIREMENT("SVC-FAULTMANAGER-001");
    FaultManagerTester tester;
    tester.testNominalResponse();
}

TEST(Nominal, PrecedenceSelection) {
    COMMENT("Multiple latched faults are responded to in precedence order");
    FaultManagerTester tester;
    tester.testPrecedenceSelection();
}

TEST(Nominal, Preemption) {
    COMMENT("A higher-precedence report cancels the active step and the preempted fault is responded to afterwards");
    FaultManagerTester tester;
    tester.testPreemption();
}

TEST(Nominal, NoPreemptionLowerPrecedence) {
    COMMENT("A lower-precedence report waits for the active response");
    FaultManagerTester tester;
    tester.testNoPreemptionLowerPrecedence();
}

TEST(Nominal, ResponseDisabled) {
    COMMENT("Disabled responses skip their steps");
    FaultManagerTester tester;
    tester.testResponseDisabled();
}

TEST(Nominal, MultiStepResponse) {
    COMMENT("A multi-step response dispatches each step in order to its configured port");
    FaultManagerTester tester;
    tester.testMultiStepResponse();
}

TEST(OffNominal, DeferThenContinue) {
    COMMENT("Failure mode DEFER runs the remaining steps before failing the response");
    FaultManagerTester tester;
    tester.testDeferThenContinue();
}

TEST(OffNominal, StepTimeout) {
    COMMENT("A step without completion within timeoutTicks is canceled and failed");
    FaultManagerTester tester;
    tester.testStepTimeout();
}

TEST(OffNominal, StepPortUnconnected) {
    COMMENT("A step whose dispatch port is unconnected fails without asserting");
    FaultManagerTester tester;
    tester.testStepPortUnconnected();
}

TEST(OffNominal, IgnoredReportThrottle) {
    COMMENT("FaultIgnored is throttled and the throttle clears every run tick");
    FaultManagerTester tester;
    tester.testIgnoredReportThrottle();
}

TEST(OffNominal, DisableClearsLatch) {
    COMMENT("Disabling a latched fault discards its pending report");
    FaultManagerTester tester;
    tester.testDisableClearsLatch();
}

TEST(Nominal, TableParameters) {
    COMMENT("RESPONSE_TABLE and STEP_TABLE parameters override the defaults when valid");
    REQUIREMENT("SVC-FAULTMANAGER-003");
    FaultManagerTester tester;
    tester.testTableParameters();
}

TEST(OffNominal, DuplicateReportIgnored) {
    COMMENT("A report of an already latched fault is ignored");
    FaultManagerTester tester;
    tester.testDuplicateReportIgnored();
}

TEST(OffNominal, InvalidReport) {
    COMMENT("An out-of-range fault id is rejected without asserting");
    FaultManagerTester tester;
    tester.testInvalidReport();
}

TEST(OffNominal, FaultDisabled) {
    COMMENT("A disabled fault is not responded to until re-enabled");
    FaultManagerTester tester;
    tester.testFaultDisabled();
}

TEST(OffNominal, StepFailureFault) {
    COMMENT("Failure mode FAULT fails the response and reports FAULT_RESPONSE_FAILURE");
    FaultManagerTester tester;
    tester.testStepFailureFault();
}

TEST(OffNominal, StepFailureIgnore) {
    COMMENT("Failure mode IGNORE continues the response");
    FaultManagerTester tester;
    tester.testStepFailureIgnore();
}

TEST(OffNominal, StepFailureDefer) {
    COMMENT("Failure mode DEFER finishes the response then reports FAULT_RESPONSE_FAILURE");
    FaultManagerTester tester;
    tester.testStepFailureDefer();
}

TEST(OffNominal, UnexpectedCompletion) {
    COMMENT("Completions not matching the active step are rejected");
    FaultManagerTester tester;
    tester.testUnexpectedCompletion();
}

TEST(OffNominal, CommandValidation) {
    COMMENT("Commands with out-of-range enumerations respond VALIDATION_ERROR");
    FaultManagerTester tester;
    tester.testCommandValidation();
}

TEST(OffNominal, ResponseFailureNoRecursion) {
    COMMENT("A failed response to FAULT_RESPONSE_FAILURE does not re-report FAULT_RESPONSE_FAILURE");
    FaultManagerTester tester;
    tester.testResponseFailureNoRecursion();
}

TEST(FaultManagerVariants, TwoStepResponse) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testTwoStepResponse();
}

TEST(FaultManagerVariants, FullLengthResponse) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testFullLengthResponse();
}

TEST(FaultManagerVariants, SharedStepAcrossResponses) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testSharedStepAcrossResponses();
}

TEST(FaultManagerVariants, SharedResponseAcrossFaults) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testSharedResponseAcrossFaults();
}

TEST(FaultManagerVariants, FailureModeMatrix) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testFailureModeMatrix();
}

TEST(FaultManagerVariants, DeferThenFault) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testDeferThenFault();
}

TEST(FaultManagerVariants, DeferThenIgnore) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testDeferThenIgnore();
}

TEST(FaultManagerVariants, EqualPrecedenceCountdown) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testEqualPrecedenceCountdown();
}

TEST(FaultManagerVariants, EqualPrecedenceNoPreempt) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testEqualPrecedenceNoPreempt();
}

TEST(FaultManagerVariants, PendingReportAfterResponse) {
    COMMENT(
        "A report latched during a response is responded to at the completion (zero countdown) or after the countdown");
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testPendingReportAfterResponse();
}

TEST(FaultManagerVariants, LowerPrecedenceDuringCountdown) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testLowerPrecedenceDuringCountdown();
}

TEST(FaultManagerVariants, CountdownNotRestarted) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testCountdownNotRestarted();
}

TEST(FaultManagerVariants, PreemptAtEachStep) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testPreemptAtEachStep();
}

TEST(FaultManagerVariants, ThreeFaultChain) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testThreeFaultChain();
}

TEST(FaultManagerVariants, FlappingReporter) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testFlappingReporter();
}

TEST(FaultManagerVariants, CompletionDuringDispatch) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testCompletionDuringDispatch();
}

TEST(FaultManagerVariants, QueueFullReportLatched) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testQueueFullReportLatched();
}

TEST(FaultManagerVariants, QueueFullSignalAsserts) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testQueueFullSignalAsserts();
}

TEST(FaultManagerVariants, DisableFaultDuringCountdown) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testDisableFaultDuringCountdown();
}

TEST(FaultManagerVariants, DisableFaultDuringResponse) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testDisableFaultDuringResponse();
}

TEST(FaultManagerVariants, DisableResponseMidResponse) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testDisableResponseMidResponse();
}

TEST(FaultManagerVariants, ParameterPersistence) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testParameterPersistence();
}

TEST(FaultManagerVariants, ParameterValidation) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testParameterValidation();
}

TEST(FaultManagerVariants, CommandInvalidEnumerations) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testCommandInvalidEnumerations();
}

TEST(FaultManagerVariants, StepPortNumPorts) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testStepPortNumPorts();
}

TEST(FaultManagerVariants, UnconnectedDispatchPort) {
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testUnconnectedDispatchPort();
}

TEST(Nominal, ParameterSave) {
    COMMENT("PRM_SAVE of the never-set RESPONSE_TABLE and STEP_TABLE persists the active tables");
    Svc::FaultProtection::FaultManagerTester tester;
    tester.testParameterSave();
}

TEST(OffNominal, ReportDroppedFromQueue) {
    COMMENT("A report whose internal message is dropped from a full queue is still responded to");
    FaultManagerTester tester;
    tester.testReportDroppedFromQueue();
}

TEST(OffNominal, PreemptionAfterDroppedReport) {
    COMMENT("A higher-precedence report whose internal message is dropped still preempts on the next tick");
    REQUIREMENT("SVC-FAULTMANAGER-016");
    FaultManagerTester tester;
    tester.testPreemptionAfterDroppedReport();
}

TEST(OffNominal, DisabledReportDroppedFromQueue) {
    COMMENT("A disabled fault's report whose internal message is dropped is discarded on the next tick");
    FaultManagerTester tester;
    tester.testDisabledReportDroppedFromQueue();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
