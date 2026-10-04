// ======================================================================
// \title  FaultManagerTester.cpp
// \author mstarch
// \brief  cpp file for FaultManager component test harness implementation class
// ======================================================================

#include "FaultManagerTester.hpp"

namespace Svc {

namespace FaultProtection {

namespace {
const FaultConfig::Fault FATAL = FaultConfig::Fault::FATAL_OCCURRED;
const FaultConfig::Fault FAILURE = FaultConfig::Fault::FAULT_RESPONSE_FAILURE;
const FaultConfig::Response REBOOT_RESPONSE = FaultConfig::Response::REBOOT_RESPONSE;
const FaultConfig::Response SEQUENCE_RESPONSE = FaultConfig::Response::SEQUENCE_RESPONSE;
const FaultConfig::Response SEQUENCE_THEN_REBOOT = FaultConfig::Response::SEQUENCE_THEN_REBOOT_RESPONSE;
const FaultConfig::Step REBOOT = FaultConfig::Step::REBOOT;
const FaultConfig::Step RUN_SEQUENCE = FaultConfig::Step::RUN_SEQUENCE;
const FaultConfig::Port REBOOT_PORT = FaultConfig::Port::REBOOT_RESPONDER_PORT;
const FaultConfig::Port SEQUENCE_PORT = FaultConfig::Port::SEQUENCE_RESPONDER_PORT;
}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FaultManagerTester ::FaultManagerTester()
    : FaultManagerGTestBase("FaultManagerTester", FaultManagerTester::MAX_HISTORY_SIZE),
      component("FaultManager"),
      m_last_dispatch_port(-1),
      m_last_cancel_port(-1),
      m_fill_queue_on_report(false) {
    this->initComponents();
    this->connectPorts();
    // The component is its own external parameter delegate: load the defaults through the tester's parameter store
    this->component.loadParameters();
    this->clearHistory();
}

FaultManagerTester ::~FaultManagerTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FaultManagerTester ::testIdle() {
    this->tick(5);
    ASSERT_EVENTS_SIZE(0);
    this->assertNotDispatched();
}

void FaultManagerTester ::testNominalResponse() {
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultReported(0, FATAL);
    ASSERT_TLM_FaultsReported(0, 1);
    this->clearHistory();

    // Report is latched: the countdown holds the response for RESPONSE_COUNTDOWN_TICKS further ticks
    this->tick(TICKS_TO_RESPONSE - 1);
    this->assertNotDispatched();
    ASSERT_EVENTS_ResponseStarted_SIZE(0);

    this->tick();
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepStarted_SIZE(1);
    ASSERT_EVENTS_StepStarted(0, REBOOT, REBOOT_RESPONSE, FATAL);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->clearHistory();

    // Further ticks while awaiting completion do nothing
    this->tick(3);
    this->assertNotDispatched();
    ASSERT_EVENTS_SIZE(0);

    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepCompleted_SIZE(1);
    ASSERT_EVENTS_StepCompleted(0, REBOOT, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FATAL);
    ASSERT_TLM_ResponsesCompleted(0, 1);
    this->clearHistory();

    // Latch cleared: a new report of the same fault is accepted and responded to again
    this->tick(3);
    this->assertNotDispatched();
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testDuplicateReportIgnored() {
    this->report(FATAL);
    this->clearHistory();
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(0);
    ASSERT_EVENTS_FaultIgnored_SIZE(1);
    ASSERT_EVENTS_FaultIgnored(0, FATAL);
    ASSERT_TLM_FaultsIgnored(0, 1);
    this->clearHistory();

    // Still latched during the response
    this->tick(TICKS_TO_RESPONSE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->report(FATAL);
    ASSERT_EVENTS_FaultIgnored_SIZE(1);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
}

void FaultManagerTester ::testInvalidReport() {
    const FaultConfig::Fault invalid(static_cast<FaultConfig::Fault::T>(FaultConfig::Fault::NUM_FAULTS));
    this->invoke_to_reportIn(0, invalid);
    // Only the diagnostic is queued for the component thread; nothing is latched
    ASSERT_EVENTS_FaultInvalid_SIZE(0);
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), 1);
    this->dispatchAll(this->component);
    ASSERT_EVENTS_FaultInvalid_SIZE(1);
    ASSERT_EVENTS_FaultInvalid(0, static_cast<U8>(FaultConfig::Fault::NUM_FAULTS));
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
}

void FaultManagerTester ::testFaultDisabled() {
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    ASSERT_EVENTS_FaultEnabledSet(0, FATAL, Fw::Enabled::DISABLED);
    this->clearHistory();

    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(0);
    ASSERT_EVENTS_FaultDisabled_SIZE(1);
    ASSERT_EVENTS_FaultDisabled(0, FATAL);
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    this->clearHistory();

    // Disabled faults do not stay latched: re-enabling then reporting responds normally
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testStepFailureFault() {
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::FAILURE, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed_SIZE(1);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::FAULT);
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_ResponseFailed(0, REBOOT_RESPONSE, FATAL);
    ASSERT_TLM_ResponsesFailed(0, 1);
    // The failure latched FAULT_RESPONSE_FAILURE, whose report is handled on the thread
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultReported(0, FAILURE);

    // Response to the failure fault follows (after the countdown, or at once when none is configured)
    this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
    this->clearHistory();

    // The original fault's latch was cleared by the failed response: idle
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_SIZE(0);
}

void FaultManagerTester ::testStepFailureIgnore() {
    this->sendCommandUpdateStepFailureMode(REBOOT, FaultConfig::FailureMode::IGNORE, Fw::CmdResponse::OK);
    ASSERT_EVENTS_StepFailureModeSet(0, REBOOT, FaultConfig::FailureMode::IGNORE);
    this->clearHistory();

    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::FAILURE, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed_SIZE(1);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::IGNORE);
    ASSERT_EVENTS_ResponseFailed_SIZE(0);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_FaultReported_SIZE(0);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
}

void FaultManagerTester ::testStepFailureDefer() {
    this->sendCommandUpdateStepFailureMode(REBOOT, FaultConfig::FailureMode::DEFER, Fw::CmdResponse::OK);
    this->clearHistory();

    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::FAILURE, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::DEFER);
    // Remaining steps are SKIP, so the response ends and the deferred failure is applied
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_ResponseFailed(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
}

void FaultManagerTester ::testResponseDisabled() {
    this->sendCommandSetResponseEnabled(REBOOT_RESPONSE, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    ASSERT_EVENTS_ResponseEnabledSet(0, REBOOT_RESPONSE, Fw::Enabled::DISABLED);
    this->clearHistory();

    this->report(FATAL);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_StepSkipped_SIZE(1);
    ASSERT_EVENTS_StepSkipped(0, REBOOT, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    this->clearHistory();

    // Latch cleared
    this->tick(TICKS_TO_RESPONSE);
    ASSERT_EVENTS_SIZE(0);
}

void FaultManagerTester ::testPreemption() {
    // FATAL (precedence 10) -> sequence response, so that FAULT_RESPONSE_FAILURE (precedence 20) can preempt it
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);

    this->report(FAILURE);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
    ASSERT_EVENTS_StepCancel_SIZE(1);
    ASSERT_EVENTS_StepCancel(0, RUN_SEQUENCE);
    ASSERT_from_stepCancelOut_SIZE(1);
    ASSERT_EQ(this->m_last_cancel_port, static_cast<FwIndexType>(SEQUENCE_PORT.e));
    ASSERT_EVENTS_ResponsePreempted_SIZE(1);
    ASSERT_EVENTS_ResponsePreempted(0, SEQUENCE_RESPONSE, FATAL, FAILURE);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    ASSERT_EVENTS_ResponseFailed_SIZE(0);

    // Late completion of the canceled step is unexpected
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(1);

    // The preempting fault is responded to after a fresh countdown (at once when none is configured)
    this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);

    // The preempted fault stayed latched and is responded to next
    this->awaitPendingResponse(SEQUENCE_RESPONSE, FATAL, SEQUENCE_PORT, RUN_SEQUENCE);
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
}

void FaultManagerTester ::testNoPreemptionLowerPrecedence() {
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    // FAULT_RESPONSE_FAILURE (20) responds first; FATAL (10) arrives during the response
    this->reportAndDispatch(FAILURE, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported(0, FATAL);
    ASSERT_EVENTS_StepCancel_SIZE(0);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->clearHistory();

    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);

    this->awaitPendingResponse(SEQUENCE_RESPONSE, FATAL, SEQUENCE_PORT, RUN_SEQUENCE);
}

void FaultManagerTester ::testPrecedenceSelection() {
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS < 1) {
        GTEST_SKIP() << "requires a non-zero RESPONSE_COUNTDOWN_TICKS";
    }
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->report(FATAL);
    this->tick();
    this->report(FAILURE);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE - 1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testUnexpectedCompletion() {
    // Idle: any completion is unexpected
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(1);
    ASSERT_EVENTS_UnexpectedStepCompleted(0, REBOOT, REBOOT_RESPONSE);
    this->clearHistory();

    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    // Wrong step and wrong response are both rejected and leave the response active
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, REBOOT);
    ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(2);
    ASSERT_EVENTS_StepCompleted_SIZE(0);
    this->clearHistory();

    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
}

void FaultManagerTester ::testCommandValidation() {
    const FaultConfig::Fault badFault(static_cast<FaultConfig::Fault::T>(FaultConfig::Fault::NUM_FAULTS));
    const FaultConfig::Response badResponse(
        static_cast<FaultConfig::Response::T>(FaultConfig::Response::NUM_RESPONSES));
    const FaultConfig::Step badStep(static_cast<FaultConfig::Step::T>(FaultConfig::Step::SKIP));

    this->sendCommandSetFaultEnabled(badFault, Fw::Enabled::DISABLED, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidFaultArgument_SIZE(1);
    ASSERT_EVENTS_InvalidFaultArgument(0, static_cast<U8>(FaultConfig::Fault::NUM_FAULTS));
    ASSERT_EVENTS_FaultEnabledSet_SIZE(0);
    this->sendCommandSetResponseEnabled(badResponse, Fw::Enabled::DISABLED, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidResponseArgument_SIZE(1);
    ASSERT_EVENTS_InvalidResponseArgument(0, static_cast<U8>(FaultConfig::Response::NUM_RESPONSES));
    ASSERT_EVENTS_ResponseEnabledSet_SIZE(0);
    this->sendCommandUpdateStepFailureMode(badStep, FaultConfig::FailureMode::IGNORE,
                                           Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidStepArgument_SIZE(1);
    ASSERT_EVENTS_InvalidStepArgument(0, static_cast<U8>(FaultConfig::Step::SKIP));
    ASSERT_EVENTS_StepFailureModeSet_SIZE(0);
    this->clearHistory();

    // Behavior unchanged by rejected commands
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testResponseFailureNoRecursion() {
    this->reportAndDispatch(FAILURE, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::FAILURE, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_ResponseFailed(0, REBOOT_RESPONSE, FAILURE);
    ASSERT_EVENTS_FaultReported_SIZE(0);
    ASSERT_EVENTS_FaultIgnored_SIZE(0);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_SIZE(0);
}

void FaultManagerTester ::testMultiStepResponse() {
    if (FaultConfig::FAULT_RESPONSE_STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    this->remapFault(FATAL, SEQUENCE_THEN_REBOOT, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_THEN_REBOOT, RUN_SEQUENCE);

    // Completing the first step dispatches the second to its own port; the response is not yet complete
    this->complete(Fw::Success::SUCCESS, SEQUENCE_THEN_REBOOT, RUN_SEQUENCE);
    ASSERT_EVENTS_StepCompleted_SIZE(1);
    ASSERT_EVENTS_StepCompleted(0, RUN_SEQUENCE, SEQUENCE_THEN_REBOOT, FATAL);
    ASSERT_EVENTS_StepStarted_SIZE(1);
    ASSERT_EVENTS_StepStarted(0, REBOOT, SEQUENCE_THEN_REBOOT, FATAL);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    this->assertDispatched(REBOOT_PORT, SEQUENCE_THEN_REBOOT, REBOOT);
    this->clearHistory();

    // A completion for the already finished step is unexpected while the second step is active
    this->complete(Fw::Success::SUCCESS, SEQUENCE_THEN_REBOOT, RUN_SEQUENCE);
    ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    this->clearHistory();

    // The trailing SKIP ends the response after the second step completes
    this->complete(Fw::Success::SUCCESS, SEQUENCE_THEN_REBOOT, REBOOT);
    ASSERT_EVENTS_StepCompleted(0, REBOOT, SEQUENCE_THEN_REBOOT, FATAL);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_THEN_REBOOT, FATAL);
    ASSERT_TLM_ResponsesCompleted(0, 1);
    this->assertNotDispatched();
}

void FaultManagerTester ::testDeferThenContinue() {
    if (FaultConfig::FAULT_RESPONSE_STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    this->remapFault(FATAL, SEQUENCE_THEN_REBOOT, 10);
    this->sendCommandUpdateStepFailureMode(RUN_SEQUENCE, FaultConfig::FailureMode::DEFER, Fw::CmdResponse::OK);
    this->clearHistory();
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_THEN_REBOOT, RUN_SEQUENCE);

    // DEFER: the failure is recorded, but the remaining steps still run
    this->complete(Fw::Success::FAILURE, SEQUENCE_THEN_REBOOT, RUN_SEQUENCE);
    ASSERT_EVENTS_StepFailed_SIZE(1);
    ASSERT_EVENTS_StepFailed(0, RUN_SEQUENCE, SEQUENCE_THEN_REBOOT, FATAL, FaultConfig::FailureMode::DEFER);
    ASSERT_EVENTS_ResponseFailed_SIZE(0);
    ASSERT_EVENTS_FaultReported_SIZE(0);
    ASSERT_EVENTS_StepStarted(0, REBOOT, SEQUENCE_THEN_REBOOT, FATAL);
    this->assertDispatched(REBOOT_PORT, SEQUENCE_THEN_REBOOT, REBOOT);
    this->clearHistory();

    // A successful final step cannot rescue the deferred failure
    this->complete(Fw::Success::SUCCESS, SEQUENCE_THEN_REBOOT, REBOOT);
    ASSERT_EVENTS_StepCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_ResponseFailed(0, SEQUENCE_THEN_REBOOT, FATAL);
    ASSERT_TLM_ResponsesFailed(0, 1);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
}

void FaultManagerTester ::testStepTimeout() {
    const StepDefinitionTable steps;
    FwSizeType timeout = 0;
    for (FwSizeType i = 0; i < StepDefinitionTable::SIZE; i++) {
        if (steps[i].get_step() == REBOOT) {
            timeout = steps[i].get_timeoutTicks();
        }
    }
    ASSERT_GT(timeout, 0) << "test requires a REBOOT step timeout";

    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);

    // The step is awaited for timeoutTicks - 1 ticks without consequence
    this->tick(timeout - 1);
    ASSERT_EVENTS_StepTimedOut_SIZE(0);
    ASSERT_EVENTS_SIZE(0);

    // The expiring tick cancels the step and fails it per its failure mode (FAULT)
    this->tick();
    ASSERT_EVENTS_StepTimedOut_SIZE(1);
    ASSERT_EVENTS_StepTimedOut(0, REBOOT, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepCancel_SIZE(1);
    ASSERT_EVENTS_StepCancel(0, REBOOT);
    ASSERT_from_stepCancelOut_SIZE(1);
    ASSERT_EQ(this->m_last_cancel_port, static_cast<FwIndexType>(REBOOT_PORT.e));
    ASSERT_EVENTS_StepFailed_SIZE(1);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::FAULT);
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_ResponseFailed(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS == 0) {
        // No countdown: the response to FAULT_RESPONSE_FAILURE starts on the expiring tick. It dispatches the same
        // (response, step) pair, so a late completion of the timed-out step is indistinguishable from the new step's
        ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
        this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
        this->clearHistory();
    } else {
        this->clearHistory();

        // A late completion of the timed-out step is unexpected
        this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
        ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(1);
        this->clearHistory();

        // A fresh dispatch starts a fresh timeout. FAULT_RESPONSE_FAILURE was latched during the expiring tick, so
        // that tick already counted toward its countdown.
        this->tick(TICKS_TO_RESPONSE - 1);
        this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
        this->clearHistory();
    }
    this->tick(timeout - 1);
    ASSERT_EVENTS_StepTimedOut_SIZE(0);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
}

void FaultManagerTester ::testStepPortUnconnected() {
    // A second instance with REBOOT_RESPONDER_PORT left unconnected; events and telemetry route to this tester
    FaultManager bare("FaultManagerBare");
    bare.init(FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH, FaultManagerTester::TEST_INSTANCE_ID + 1);
    bare.set_logOut_OutputPort(0, this->get_from_logOut(0));
#if FW_ENABLE_TEXT_LOGGING == 1
    bare.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
#endif
    bare.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
    bare.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    bare.set_stepDispatchOut_OutputPort(static_cast<FwIndexType>(SEQUENCE_PORT.e),
                                        this->get_from_stepDispatchOut(static_cast<FwIndexType>(SEQUENCE_PORT.e)));
    bare.set_stepCancelOut_OutputPort(static_cast<FwIndexType>(SEQUENCE_PORT.e),
                                      this->get_from_stepCancelOut(static_cast<FwIndexType>(SEQUENCE_PORT.e)));
    ASSERT_FALSE(bare.isConnected_stepDispatchOut_OutputPort(static_cast<FwIndexType>(REBOOT_PORT.e)));
    this->clearHistory();

    bare.get_reportIn_InputPort(0)->invoke(FATAL);
    this->dispatchAll(bare);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    this->clearHistory();
    for (FwSizeType i = 0; i < TICKS_TO_RESPONSE; i++) {
        bare.get_run_InputPort(0)->invoke(0);
        this->dispatchAll(bare);
    }

    // The step cannot be dispatched: it fails without a port call and the response fails per the step's mode.
    // Without a countdown the response to FAULT_RESPONSE_FAILURE (the same unconnected step) starts and fails on the
    // same tick, exhausting the escalation.
    const FwSizeType failures = (FaultConfig::RESPONSE_COUNTDOWN_TICKS == 0) ? 2 : 1;
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepStarted(0, REBOOT, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepPortUnconnected_SIZE(failures);
    ASSERT_EVENTS_StepPortUnconnected(0, REBOOT, REBOOT_PORT);
    this->assertNotDispatched();
    ASSERT_from_stepCancelOut_SIZE(0);
    ASSERT_EVENTS_StepFailed_SIZE(failures);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::FAULT);
    ASSERT_EVENTS_ResponseFailed_SIZE(failures);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
    ASSERT_EVENTS_EscalationExhausted_SIZE(failures - 1);
    bare.deinit();
}

void FaultManagerTester ::testIgnoredReportThrottle() {
    this->report(FATAL);
    this->clearHistory();

    // Reports beyond the throttle are latched-and-ignored silently
    const FwSizeType throttle = FaultManagerComponentBase::EVENTID_FAULTIGNORED_THROTTLE;
    for (FwSizeType i = 0; i < throttle + 2; i++) {
        this->report(FATAL);
    }
    ASSERT_EVENTS_FaultIgnored_SIZE(throttle);
    this->clearHistory();

    // Each run tick clears the throttle
    this->tick();
    this->report(FATAL);
    ASSERT_EVENTS_FaultIgnored_SIZE(1);
    ASSERT_EVENTS_FaultIgnored(0, FATAL);
    this->clearHistory();

    // FaultInvalid is throttled and cleared the same way
    const FaultConfig::Fault badFault(static_cast<FaultConfig::Fault::T>(FaultConfig::Fault::NUM_FAULTS));
    const FwSizeType invalidThrottle = FaultManagerComponentBase::EVENTID_FAULTINVALID_THROTTLE;
    for (FwSizeType i = 0; i < invalidThrottle + 2; i++) {
        this->report(badFault);
    }
    ASSERT_EVENTS_FaultInvalid_SIZE(invalidThrottle);
    this->clearHistory();
    this->tick();
    this->report(badFault);
    ASSERT_EVENTS_FaultInvalid_SIZE(1);
}

void FaultManagerTester ::testDisableClearsLatch() {
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    this->clearHistory();

    // Disabling a latched fault discards its pending report
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_ResponseStarted_SIZE(0);

    // Re-enabling does not resurrect the discarded report
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_SIZE(0);

    // A fresh report is accepted (the latch was cleared) and responded to
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testParameterSave() {
    // Ground changes to the enabled responses and step failure modes
    this->sendCommandSetResponseEnabled(REBOOT_RESPONSE, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    this->sendCommandUpdateStepFailureMode(RUN_SEQUENCE, FaultConfig::FailureMode::IGNORE, Fw::CmdResponse::OK);

    // The tester base asserts that each saved value equals its own copy: set that copy to the expected active tables
    // (never loaded by the component: the parameters are at their model defaults)
    ResponsesEnabled responses;
    responses[REBOOT_RESPONSE.e] = Fw::Enabled::DISABLED;
    StepFailureModes steps;
    const StepDefinitionTable definitions;
    for (FwSizeType i = 0; i < StepDefinitionTable::SIZE; i++) {
        const StepDefinitionEntry& entry = definitions[i];
        if (entry.get_step() != FaultConfig::Step::SKIP) {
            steps[entry.get_step()] = entry.get_failureMode();
        }
    }
    steps[RUN_SEQUENCE.e] = FaultConfig::FailureMode::IGNORE;
    this->paramSet_RESPONSE_TABLE(responses, Fw::ParamValid::VALID);
    this->paramSet_STEP_TABLE(steps, Fw::ParamValid::VALID);
    this->clearHistory();

    // Never set nor loaded from a database: the model defaults make the parameters valid, so the saves are accepted
    this->paramSave_RESPONSE_TABLE(0, 7);
    this->paramSave_STEP_TABLE(0, 8);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_RESPONSE_TABLE_SAVE, 7, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, FaultManager::OPCODE_STEP_TABLE_SAVE, 8, Fw::CmdResponse::OK);

    // Revert the live tables, then reload from the (tester's) parameter database: the saved tables come back
    this->sendCommandSetResponseEnabled(REBOOT_RESPONSE, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->sendCommandUpdateStepFailureMode(RUN_SEQUENCE, FaultConfig::FailureMode::FAULT, Fw::CmdResponse::OK);
    this->component.loadParameters();
    this->clearHistory();
    this->report(FATAL);
    ASSERT_EVENTS_StepSkipped_SIZE(0);
    this->tick(TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepSkipped_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    this->clearHistory();
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_StepFailed(0, RUN_SEQUENCE, SEQUENCE_RESPONSE, FATAL, FaultConfig::FailureMode::IGNORE);
}

void FaultManagerTester ::testTableParameters() {
    // Valid RESPONSE_TABLE and STEP_TABLE parameters override the FaultConfig defaults
    ResponsesEnabled responses;
    responses[REBOOT_RESPONSE.e] = Fw::Enabled::DISABLED;
    StepFailureModes steps;
    steps[RUN_SEQUENCE.e] = FaultConfig::FailureMode::IGNORE;
    steps[REBOOT.e] = FaultConfig::FailureMode::IGNORE;
    this->paramSet_RESPONSE_TABLE(responses, Fw::ParamValid::VALID);
    this->paramSet_STEP_TABLE(steps, Fw::ParamValid::VALID);
    this->component.loadParameters();
    this->clearHistory();

    // REBOOT_RESPONSE disabled by parameter: its step is skipped
    this->report(FATAL);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_StepSkipped_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    this->clearHistory();

    // REBOOT failure mode IGNORE by parameter: a failed step completes the response
    this->sendCommandSetResponseEnabled(REBOOT_RESPONSE, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::FAILURE, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::IGNORE);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_FaultReported_SIZE(0);
    this->clearHistory();

    // Invalid parameters leave the cached tables untouched
    this->paramSet_RESPONSE_TABLE(responses, Fw::ParamValid::INVALID);
    this->paramSet_STEP_TABLE(steps, Fw::ParamValid::INVALID);
    this->component.loadParameters();
    this->clearHistory();
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::FAILURE, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::IGNORE);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void FaultManagerTester ::from_stepDispatchOut_handler(FwIndexType portNum,
                                                       const FaultConfig::Response& response,
                                                       const FaultConfig::Step& step,
                                                       const FaultConfig::Context& context) {
    this->m_last_dispatch_port = portNum;
    this->pushFromPortEntry_stepDispatchOut(response, step, context);
}

void FaultManagerTester ::from_stepCancelOut_handler(FwIndexType portNum) {
    this->m_last_cancel_port = portNum;
    this->pushFromPortEntry_stepCancelOut();
}

void FaultManagerTester ::logIn_ACTIVITY_HI_FaultReported(const FaultConfig::Fault& fault) {
    FaultManagerGTestBase::logIn_ACTIVITY_HI_FaultReported(fault);
    if (this->m_fill_queue_on_report) {
        // Called from within handleReport: fill the queue before the handler sends a state machine signal
        this->fillQueue();
    }
}

void FaultManagerTester ::textLogIn(FwEventIdType id,
                                    const Fw::Time& timeTag,
                                    const Fw::LogSeverity severity,
                                    const Fw::TextLogString& text) {
    TextLogEntry e = {id, timeTag, severity, text};
    printTextLogHistoryEntry(e, stdout);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void FaultManagerTester ::dispatchAll(FaultManager& target) {
    // Bounded: each dispatched message queues at most one further message and a response has finitely many steps
    for (FwSizeType i = 0; i < TEST_INSTANCE_QUEUE_DEPTH; i++) {
        if (target.m_queue.getMessagesAvailable() == 0) {
            return;
        }
        ASSERT_EQ(target.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    }
    ASSERT_EQ(target.m_queue.getMessagesAvailable(), 0);
}

void FaultManagerTester ::report(const FaultConfig::Fault& fault) {
    this->invoke_to_reportIn(0, fault);
    this->dispatchAll(this->component);
}

void FaultManagerTester ::tick(FwSizeType count) {
    for (FwSizeType i = 0; i < count; i++) {
        this->invoke_to_run(0, 0);
        this->dispatchAll(this->component);
    }
}

void FaultManagerTester ::fillQueue() {
    for (FwSizeType i = 0; i < TEST_INSTANCE_QUEUE_DEPTH; i++) {
        this->invoke_to_run(0, 0);
    }
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), static_cast<FwSizeType>(TEST_INSTANCE_QUEUE_DEPTH));
}

void FaultManagerTester ::drainQueue() {
    // Each dispatched message queues at most one further message (a Tick, a step signal, or a Preempt)
    for (FwSizeType i = 0; i < 4 * TEST_INSTANCE_QUEUE_DEPTH; i++) {
        if (this->component.m_queue.getMessagesAvailable() == 0) {
            return;
        }
        ASSERT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    }
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), 0);
}

void FaultManagerTester ::testReportDroppedFromQueue() {
    this->fillQueue();
    this->invoke_to_reportIn(0, FATAL);
    // The handleReport message was dropped, yet the report is latched
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), static_cast<FwSizeType>(TEST_INSTANCE_QUEUE_DEPTH));
    this->drainQueue();
    ASSERT_EVENTS_FaultReported_SIZE(0);
    ASSERT_EVENTS_FaultIgnored_SIZE(0);
    // The drained ticks find the latched report and count toward the countdown; a countdown longer than the queue
    // depth needs the remaining ticks before the response starts
    if (TICKS_TO_RESPONSE > TEST_INSTANCE_QUEUE_DEPTH) {
        ASSERT_EVENTS_ResponseStarted_SIZE(0);
        this->tick(TICKS_TO_RESPONSE - TEST_INSTANCE_QUEUE_DEPTH);
    }
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);

    // The response completes and clears the latch as usual
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    this->clearHistory();
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(1);
}

void FaultManagerTester ::testPreemptionAfterDroppedReport() {
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);

    this->fillQueue();
    this->invoke_to_reportIn(0, FAILURE);
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), static_cast<FwSizeType>(TEST_INSTANCE_QUEUE_DEPTH));
    this->drainQueue();
    ASSERT_EVENTS_FaultReported_SIZE(0);

    // The tick audit finds the latched higher-precedence fault and preempts, exactly once
    ASSERT_EVENTS_StepCancel_SIZE(1);
    ASSERT_EVENTS_StepCancel(0, RUN_SEQUENCE);
    ASSERT_from_stepCancelOut_SIZE(1);
    ASSERT_EQ(this->m_last_cancel_port, static_cast<FwIndexType>(SEQUENCE_PORT.e));
    ASSERT_EVENTS_ResponsePreempted_SIZE(1);
    ASSERT_EVENTS_ResponsePreempted(0, SEQUENCE_RESPONSE, FATAL, FAILURE);
    ASSERT_EVENTS_StepTimedOut_SIZE(0);
    ASSERT_EVENTS_ResponseFailed_SIZE(0);

    // The preempting fault is responded to after a fresh countdown; without a configured countdown its response
    // already started within the drained ticks
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS > 0) {
        this->clearHistory();
        this->tick(TICKS_TO_RESPONSE);
    }
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testDisabledReportDroppedFromQueue() {
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    this->clearHistory();

    this->fillQueue();
    this->invoke_to_reportIn(0, FATAL);
    this->drainQueue();
    // The tick audit discards the report of the disabled fault as the dropped handler would have
    ASSERT_EVENTS_FaultDisabled_SIZE(1);
    ASSERT_EVENTS_FaultDisabled(0, FATAL);
    this->assertNotDispatched();
    this->clearHistory();

    // Re-enabling the fault does not resurrect the discarded report
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    this->assertNotDispatched();
}

void FaultManagerTester ::complete(const Fw::Success& status,
                                   const FaultConfig::Response& response,
                                   const FaultConfig::Step& step) {
    this->invoke_to_stepCompletionIn(0, status, response, step);
    this->dispatchAll(this->component);
}

void FaultManagerTester ::assertDispatched(const FaultConfig::Port& port,
                                           const FaultConfig::Response& response,
                                           const FaultConfig::Step& step) {
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->size(), 1) << "expected exactly one step dispatch";
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->at(0).response, response);
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->at(0).step, step);
    ASSERT_EQ(this->m_last_dispatch_port, static_cast<FwIndexType>(port.e));
    this->clearFromPortHistory();
}

void FaultManagerTester ::assertNotDispatched() {
    ASSERT_from_stepDispatchOut_SIZE(0);
}

void FaultManagerTester ::tickWithinTimeout(const FaultConfig::Step& step) {
    // Enough ticks to cover a full countdown, bounded so the active step does not time out
    FwSizeType ticks = FaultManagerTester::TICKS_TO_RESPONSE;
    const StepDefinitionTable steps;
    for (FwSizeType i = 0; i < StepDefinitionTable::SIZE; i++) {
        const U32 timeout = steps[i].get_timeoutTicks();
        if ((steps[i].get_step() == step) && (timeout > 0) && (static_cast<FwSizeType>(timeout) <= ticks)) {
            ticks = static_cast<FwSizeType>(timeout) - 1;
        }
    }
    this->tick(ticks);
}

void FaultManagerTester ::awaitPendingResponse(const FaultConfig::Response& response,
                                               const FaultConfig::Fault& fault,
                                               const FaultConfig::Port& port,
                                               const FaultConfig::Step& step) {
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS > 0) {
        this->tick(FaultConfig::RESPONSE_COUNTDOWN_TICKS);
    }
    ASSERT_GT(this->eventHistory_ResponseStarted->size(), 0u) << "pending response did not start";
    ASSERT_EVENTS_ResponseStarted(this->eventHistory_ResponseStarted->size() - 1, response, fault);
    this->assertDispatched(port, response, step);
    this->clearHistory();
}

void FaultManagerTester ::reportAndDispatch(const FaultConfig::Fault& fault,
                                            const FaultConfig::Port& port,
                                            const FaultConfig::Response& response,
                                            const FaultConfig::Step& step) {
    this->report(fault);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    this->clearHistory();
    this->tick(TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, response, fault);
    this->assertDispatched(port, response, step);
    this->clearHistory();
}

void FaultManagerTester ::remapFault(const FaultConfig::Fault& fault,
                                     const FaultConfig::Response& response,
                                     U8 precedence) {
    for (FwSizeType i = 0; i < FaultResponseTable::SIZE; i++) {
        if (this->m_fault_table[i].get_fault() == fault) {
            this->m_fault_table[i].set_response(response);
            this->m_fault_table[i].set_precedence(precedence);
        }
    }
    this->paramSet_FAULT_RESPONSE_TABLE(this->m_fault_table, Fw::ParamValid::VALID);
    this->component.loadParameters();
    this->clearHistory();
}

void FaultManagerTester ::sendCommandSetFaultEnabled(const FaultConfig::Fault& fault,
                                                     const Fw::Enabled& enabled,
                                                     const Fw::CmdResponse& expected) {
    this->clearHistory();
    this->sendCmd_SET_FAULT_ENABLED(0, 1, fault, enabled);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_SET_FAULT_ENABLED, 1, expected);
}

void FaultManagerTester ::sendCommandSetResponseEnabled(const FaultConfig::Response& response,
                                                        const Fw::Enabled& enabled,
                                                        const Fw::CmdResponse& expected) {
    this->clearHistory();
    this->sendCmd_SET_RESPONSE_ENABLED(0, 2, response, enabled);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_SET_RESPONSE_ENABLED, 2, expected);
}

void FaultManagerTester ::sendCommandUpdateStepFailureMode(const FaultConfig::Step& step,
                                                           const FaultConfig::FailureMode& mode,
                                                           const Fw::CmdResponse& expected) {
    this->clearHistory();
    this->sendCmd_UPDATE_STEP_FAILURE_MODE(0, 3, step, mode);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_UPDATE_STEP_FAILURE_MODE, 3, expected);
}

// ----------------------------------------------------------------------
// Variant test support
// ----------------------------------------------------------------------

namespace {

//! Assert hook that counts assertions rather than terminating the test
class CountingAssertHook : public Fw::AssertHook {
  public:
    CountingAssertHook() : m_count(0) {}
    void reportAssert(FILE_NAME_ARG file,
                      FwSizeType lineNo,
                      FwSizeType numArgs,
                      FwAssertArgType arg1,
                      FwAssertArgType arg2,
                      FwAssertArgType arg3,
                      FwAssertArgType arg4,
                      FwAssertArgType arg5,
                      FwAssertArgType arg6) override {
        this->m_count++;
    }
    void doAssert() override {}
    U32 count() const { return this->m_count; }

  private:
    U32 m_count;
};

const FaultConfig::Step SKIP(FaultConfig::Step::SKIP);

}  // namespace

void FaultManagerTester ::setPrecedence(const FaultConfig::Fault& fault, U8 precedence) {
    for (FwSizeType i = 0; i < FaultResponseTable::SIZE; i++) {
        if (this->m_fault_table[i].get_fault() == fault) {
            this->remapFault(fault, this->m_fault_table[i].get_response(), precedence);
            return;
        }
    }
    FAIL() << "fault not in the table";
}

void FaultManagerTester ::assertActiveTables(const FaultResponseTable& faults,
                                             const ResponsesEnabled& responses,
                                             const StepFailureModes& steps) {
    this->paramSet_FAULT_RESPONSE_TABLE(faults, Fw::ParamValid::VALID);
    this->paramSet_RESPONSE_TABLE(responses, Fw::ParamValid::VALID);
    this->paramSet_STEP_TABLE(steps, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSave_FAULT_RESPONSE_TABLE(0, 40);
    this->paramSave_RESPONSE_TABLE(0, 41);
    this->paramSave_STEP_TABLE(0, 42);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(3);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SAVE, 40, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, FaultManager::OPCODE_RESPONSE_TABLE_SAVE, 41, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(2, FaultManager::OPCODE_STEP_TABLE_SAVE, 42, Fw::CmdResponse::OK);
    this->clearHistory();
}

StepFailureModes FaultManagerTester ::definedStepModes() {
    StepFailureModes steps;
    const StepDefinitionTable definitions;
    for (FwSizeType i = 0; i < StepDefinitionTable::SIZE; i++) {
        const StepDefinitionEntry& entry = definitions[i];
        if (entry.get_step() != FaultConfig::Step::SKIP) {
            steps[entry.get_step()] = entry.get_failureMode();
        }
    }
    return steps;
}

// ----------------------------------------------------------------------
// Variant tests: multi-step responses
// ----------------------------------------------------------------------

void FaultManagerTester ::testSharedResponseAcrossFaults() {
    // Default table: FATAL_OCCURRED (10) and FAULT_RESPONSE_FAILURE (20) both map to REBOOT_RESPONSE
    this->report(FATAL);
    this->report(FAILURE);
    ASSERT_EVENTS_FaultReported_SIZE(2);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
    this->clearHistory();
    // Completing the shared response clears the latch of every fault mapped to it: FATAL_OCCURRED is not
    // responded to again (see clearLatchesForResponse)
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(0);
    this->assertNotDispatched();
    // Both faults may be reported anew
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultIgnored_SIZE(0);
}

// ----------------------------------------------------------------------
// Variant tests: failure modes
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// Variant tests: precedence and preemption
// ----------------------------------------------------------------------

void FaultManagerTester ::testEqualPrecedenceCountdown() {
    // Both faults at precedence 20, distinct responses
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 20);
    this->report(FAILURE);
    this->report(FATAL);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    // Ties resolve to the earliest table entry (FATAL_OCCURRED), regardless of report order
    ASSERT_EVENTS_ResponseStarted(0, SEQUENCE_RESPONSE, FATAL);
    this->assertDispatched(SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
}

void FaultManagerTester ::testEqualPrecedenceNoPreempt() {
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 20);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->report(FAILURE);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    ASSERT_from_stepCancelOut_SIZE(0);
    this->tickWithinTimeout(RUN_SEQUENCE);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
}

void FaultManagerTester ::testPendingReportAfterResponse() {
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 20);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    // Equal precedence: latched for after the active response
    this->report(FAILURE);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS == 0) {
        // No countdown: the pending report's response starts on the completion itself, without another tick
        ASSERT_EVENTS_ResponseStarted_SIZE(1);
    } else {
        // The countdown restarts at the completion; the response starts RESPONSE_COUNTDOWN_TICKS ticks later
        ASSERT_EVENTS_ResponseStarted_SIZE(0);
        this->tick(FaultConfig::RESPONSE_COUNTDOWN_TICKS - 1);
        ASSERT_EVENTS_ResponseStarted_SIZE(0);
        this->tick(1);
        ASSERT_EVENTS_ResponseStarted_SIZE(1);
    }
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testLowerPrecedenceDuringCountdown() {
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS < 1) {
        GTEST_SKIP() << "requires a non-zero RESPONSE_COUNTDOWN_TICKS";
    }
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->report(FAILURE);
    this->tick(1);
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(2);
    ASSERT_EVENTS_ResponseStarted_SIZE(0);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE - 1);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    this->clearHistory();
    // The lower-precedence fault is still latched and gets its response next
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, SEQUENCE_RESPONSE, FATAL);
    this->assertDispatched(SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
}

void FaultManagerTester ::testCountdownNotRestarted() {
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS < 1) {
        GTEST_SKIP() << "requires a non-zero RESPONSE_COUNTDOWN_TICKS";
    }
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->report(FATAL);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE - 1);
    ASSERT_EVENTS_ResponseStarted_SIZE(0);
    this->report(FAILURE);
    // One more tick ends the original countdown: the new report did not extend it
    this->tick(1);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
}

void FaultManagerTester ::testThreeFaultChain() {
    if (FaultConfig::Fault::NUM_FAULTS < 3) {
        GTEST_SKIP() << "requires a FaultConfig with at least three faults";
    }
    const FaultConfig::Fault THIRD(static_cast<FaultConfig::Fault::T>(2));
    // Precedence chain: FATAL (10, sequence) < THIRD (15, reboot) < FAILURE (20, reboot)
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->remapFault(THIRD, REBOOT_RESPONSE, 15);
    this->remapFault(FAILURE, REBOOT_RESPONSE, 20);

    // Chain 1: lowest active, middle preempts, highest arrives during the countdown
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->report(THIRD);
    ASSERT_EVENTS_ResponsePreempted_SIZE(1);
    ASSERT_EVENTS_ResponsePreempted(0, SEQUENCE_RESPONSE, FATAL, THIRD);
    ASSERT_from_stepCancelOut_SIZE(1);
    this->clearHistory();
    this->tick(1);
    this->report(FAILURE);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
    this->clearHistory();
    // THIRD shares the reboot response: its latch was cleared with it. FATAL is responded to next.
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, SEQUENCE_RESPONSE, FATAL);
    this->assertDispatched(SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_SIZE(0);
    this->assertNotDispatched();

    // Chain 2: highest active, middle and lowest arrive and wait
    this->reportAndDispatch(FAILURE, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->report(THIRD);
    this->report(FATAL);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    ASSERT_from_stepCancelOut_SIZE(0);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, SEQUENCE_RESPONSE, FATAL);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);

    // Chain 3: middle active, lowest does not preempt, highest does
    this->clearHistory();
    this->reportAndDispatch(THIRD, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->report(FATAL);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->report(FAILURE);
    ASSERT_EVENTS_ResponsePreempted_SIZE(1);
    ASSERT_EVENTS_ResponsePreempted(0, REBOOT_RESPONSE, THIRD, FAILURE);
    // RebootResponder refuses cancellation: FaultManager still requests it and moves on
    ASSERT_from_stepCancelOut_SIZE(1);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FAILURE);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted(0, SEQUENCE_RESPONSE, FATAL);
}

// ----------------------------------------------------------------------
// Variant tests: latching and timing
// ----------------------------------------------------------------------

void FaultManagerTester ::testFlappingReporter() {
    const FwSizeType REPORTS_PER_TICK = 7;  // exceeds the FaultIgnored throttle of 5
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    FwSizeType ignored = 0;
    for (FwSizeType tick = 0; tick < FaultManagerTester::TICKS_TO_RESPONSE - 1; tick++) {
        this->clearHistory();
        for (FwSizeType i = 0; i < REPORTS_PER_TICK; i++) {
            this->report(FATAL);
            ignored++;
        }
        ASSERT_EVENTS_FaultReported_SIZE(0);
        // Events are throttled, telemetry is not
        ASSERT_LE(this->eventHistory_FaultIgnored->size(), 5u);
        ASSERT_TLM_FaultsIgnored(this->tlmHistory_FaultsIgnored->size() - 1, static_cast<U32>(ignored));
        ASSERT_TLM_FaultsReported(this->tlmHistory_FaultsReported->size() - 1, 1);
        this->tick(1);
        ASSERT_EVENTS_ResponseStarted_SIZE(0);
    }
    this->clearHistory();
    this->tick(1);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    // Still flapping during the response
    this->clearHistory();
    this->report(FATAL);
    ASSERT_EVENTS_FaultIgnored_SIZE(1);
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    this->clearHistory();
    // Latch cleared with the response: the next report is a new fault
    this->report(FATAL);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_TLM_FaultsReported(this->tlmHistory_FaultsReported->size() - 1, 2);
}

void FaultManagerTester ::testQueueFullSignalAsserts() {
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    // A higher-precedence report processed while the queue is full cannot enqueue the Preempt signal
    this->m_fill_queue_on_report = true;
    this->invoke_to_reportIn(0, FAILURE);
    CountingAssertHook hook;
    hook.registerHook();
    ASSERT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    hook.deregisterHook();
    this->m_fill_queue_on_report = false;
    ASSERT_EQ(hook.count(), 1u);
    // The preemption was lost: the active step was never cancelled
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    ASSERT_from_stepCancelOut_SIZE(0);
    this->drainQueue();
    ASSERT_EVENTS_ResponsePreempted_SIZE(0);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
}

// ----------------------------------------------------------------------
// Variant tests: enable/disable
// ----------------------------------------------------------------------

void FaultManagerTester ::testDisableFaultDuringResponse() {
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    // The in-flight response runs to completion
    ASSERT_from_stepCancelOut_SIZE(0);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FATAL);
    this->clearHistory();
    this->report(FATAL);
    ASSERT_EVENTS_FaultDisabled_SIZE(1);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(0);
    this->assertNotDispatched();
}

void FaultManagerTester ::sendRawCommand(FwOpcodeType opcode,
                                         U32 cmdSeq,
                                         Fw::CmdArgBuffer& args,
                                         const Fw::CmdResponse& expected) {
    this->clearHistory();
    Fw::InputCmdPort* const port = this->component.get_cmdIn_InputPort(0);
    ASSERT_NE(port, nullptr);
    port->invoke(static_cast<FwOpcodeType>(this->component.getIdBase() + opcode), cmdSeq, args);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, opcode, cmdSeq, expected);
}

void FaultManagerTester ::testParameterPersistence() {
    // FAULT_RESPONSE_TABLE: a commanded change is persisted by PRM_SAVE (RESPONSE_TABLE / STEP_TABLE:
    // testParameterSave)
    FaultResponseTable expected_faults = this->m_fault_table;
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    for (FwSizeType i = 0; i < FaultResponseTable::SIZE; i++) {
        if (expected_faults[i].get_fault() == FATAL) {
            expected_faults[i].set_enabled(Fw::Enabled::DISABLED);
        }
    }
    this->paramSet_FAULT_RESPONSE_TABLE(expected_faults, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSave_FAULT_RESPONSE_TABLE(0, 10);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SAVE, 10, Fw::CmdResponse::OK);

    // PRM_SET paths: ground-set tables take effect
    ResponsesEnabled responses(Fw::Enabled(Fw::Enabled::ENABLED));
    this->paramSet_RESPONSE_TABLE(responses, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_RESPONSE_TABLE(0, 13);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_RESPONSE_TABLE_SET, 13, Fw::CmdResponse::OK);
    StepFailureModes modes(FaultConfig::FailureMode(FaultConfig::FailureMode::DEFER));
    this->paramSet_STEP_TABLE(modes, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_STEP_TABLE(0, 14);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_STEP_TABLE_SET, 14, Fw::CmdResponse::OK);
    FaultResponseTable faults = expected_faults;
    faults[0].set_enabled(Fw::Enabled::ENABLED);
    faults[0].set_response(SEQUENCE_RESPONSE);
    this->paramSet_FAULT_RESPONSE_TABLE(faults, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, 15);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, 15, Fw::CmdResponse::OK);
    this->clearHistory();
    // Effect: FATAL re-enabled and mapped to the (re-enabled) sequence response, whose step failure now defers
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_StepFailed(0, RUN_SEQUENCE, SEQUENCE_RESPONSE, FATAL, FaultConfig::FailureMode::DEFER);
}

void FaultManagerTester ::testParameterValidation() {
    const FaultResponseTable original = this->m_fault_table;
    U32 seq = 20;

    // Response id out of range
    FaultResponseTable table = original;
    table[0].set_response(FaultConfig::Response(FaultConfig::Response::NUM_RESPONSES));
    this->paramSet_FAULT_RESPONSE_TABLE(table, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, seq);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, seq++, Fw::CmdResponse::VALIDATION_ERROR);

    // Fault id out of range
    table = original;
    table[1].set_fault(FaultConfig::Fault(FaultConfig::Fault::NUM_FAULTS));
    this->paramSet_FAULT_RESPONSE_TABLE(table, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, seq);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, seq++, Fw::CmdResponse::VALIDATION_ERROR);

    // Invalid enabled value in RESPONSE_TABLE (rejected by enumeration deserialization)
    {
        Fw::CmdArgBuffer raw;
        for (FwSizeType k = 0; k < ResponsesEnabled::SIZE; k++) {
            ASSERT_EQ(raw.serializeFrom(static_cast<U8>((k == 0) ? 7 : Fw::Enabled::ENABLED)), Fw::FW_SERIALIZE_OK);
        }
        this->sendRawCommand(FaultManager::OPCODE_RESPONSE_TABLE_SET, seq++, raw, Fw::CmdResponse::VALIDATION_ERROR);
    }

    // Invalid failure mode in STEP_TABLE (rejected by enumeration deserialization)
    {
        Fw::CmdArgBuffer raw;
        for (FwSizeType k = 0; k < StepFailureModes::SIZE; k++) {
            ASSERT_EQ(raw.serializeFrom(static_cast<I32>((k == 0) ? 9 : FaultConfig::FailureMode::FAULT)),
                      Fw::FW_SERIALIZE_OK);
        }
        this->sendRawCommand(FaultManager::OPCODE_STEP_TABLE_SET, seq++, raw, Fw::CmdResponse::VALIDATION_ERROR);
    }

    // Short buffer
    {
        Fw::CmdArgBuffer raw;
        ASSERT_EQ(raw.serializeFrom(static_cast<U8>(Fw::Enabled::ENABLED)), Fw::FW_SERIALIZE_OK);
        this->sendRawCommand(FaultManager::OPCODE_RESPONSE_TABLE_SET, seq++, raw, Fw::CmdResponse::VALIDATION_ERROR);
    }

    // A rejected PRM_SET leaves the active table untouched (deserializeParam) but the generated base class marks the
    // parameter INVALID, so PRM_SAVE of the unchanged table is refused until a valid PRM_SET arrives (report: G2)
    const ResponsesEnabled default_responses;
    const StepFailureModes defined_steps = FaultManagerTester::definedStepModes();
    this->clearHistory();
    this->paramSave_FAULT_RESPONSE_TABLE(0, seq);
    this->paramSave_RESPONSE_TABLE(0, seq + 1);
    this->paramSave_STEP_TABLE(0, seq + 2);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(3);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SAVE, seq, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_CMD_RESPONSE(1, FaultManager::OPCODE_RESPONSE_TABLE_SAVE, seq + 1, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_CMD_RESPONSE(2, FaultManager::OPCODE_STEP_TABLE_SAVE, seq + 2, Fw::CmdResponse::VALIDATION_ERROR);
    seq += 3;
    // A valid PRM_SET of the unchanged tables restores PRM_SAVE; the saved values are the construction-time tables
    this->paramSet_FAULT_RESPONSE_TABLE(original, Fw::ParamValid::VALID);
    this->paramSet_RESPONSE_TABLE(default_responses, Fw::ParamValid::VALID);
    this->paramSet_STEP_TABLE(defined_steps, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, seq);
    this->paramSend_RESPONSE_TABLE(0, seq + 1);
    this->paramSend_STEP_TABLE(0, seq + 2);
    this->dispatchAll(this->component);
    ASSERT_CMD_RESPONSE_SIZE(3);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, seq, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, FaultManager::OPCODE_RESPONSE_TABLE_SET, seq + 1, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(2, FaultManager::OPCODE_STEP_TABLE_SET, seq + 2, Fw::CmdResponse::OK);
    seq += 3;
    this->assertActiveTables(original, default_responses, defined_steps);

    // Behavior is unchanged
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

// ----------------------------------------------------------------------
// Variant tests: input validation
// ----------------------------------------------------------------------

void FaultManagerTester ::testCommandInvalidEnumerations() {
    U32 seq = 30;

    // Values outside the enumerations are rejected when the arguments are deserialized
    struct RawCase {
        FwOpcodeType opcode;
        U8 first;
        I32 second;
        bool second_is_u8;
    };
    const RawCase raw_cases[] = {
        {FaultManager::OPCODE_SET_FAULT_ENABLED, 255, Fw::Enabled::DISABLED, true},
        {FaultManager::OPCODE_SET_FAULT_ENABLED, FATAL.e, 7, true},
        {FaultManager::OPCODE_SET_RESPONSE_ENABLED, 255, Fw::Enabled::DISABLED, true},
        {FaultManager::OPCODE_SET_RESPONSE_ENABLED, REBOOT_RESPONSE.e, 7, true},
        {FaultManager::OPCODE_UPDATE_STEP_FAILURE_MODE, 255, FaultConfig::FailureMode::IGNORE, false},
        {FaultManager::OPCODE_UPDATE_STEP_FAILURE_MODE, REBOOT.e, 9, false},
    };
    for (const RawCase& raw_case : raw_cases) {
        Fw::CmdArgBuffer raw;
        ASSERT_EQ(raw.serializeFrom(raw_case.first), Fw::FW_SERIALIZE_OK);
        if (raw_case.second_is_u8) {
            ASSERT_EQ(raw.serializeFrom(static_cast<U8>(raw_case.second)), Fw::FW_SERIALIZE_OK);
        } else {
            ASSERT_EQ(raw.serializeFrom(raw_case.second), Fw::FW_SERIALIZE_OK);
        }
        this->sendRawCommand(raw_case.opcode, seq++, raw, Fw::CmdResponse::FORMAT_ERROR);
        ASSERT_EVENTS_SIZE(0);
    }

    // Short argument buffers
    {
        Fw::CmdArgBuffer raw;
        ASSERT_EQ(raw.serializeFrom(FATAL.e), Fw::FW_SERIALIZE_OK);
        this->sendRawCommand(FaultManager::OPCODE_SET_FAULT_ENABLED, seq++, raw, Fw::CmdResponse::FORMAT_ERROR);
    }

    // Nothing changed
    this->assertActiveTables(this->m_fault_table, ResponsesEnabled(), FaultManagerTester::definedStepModes());
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testUnconnectedDispatchPort() {
    // A second instance with only the sequence responder port connected
    FaultManager bare("bare");
    bare.init(FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH, 1);
    bare.set_logOut_OutputPort(0, this->get_from_logOut(0));
    bare.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
    bare.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
    bare.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    bare.set_stepDispatchOut_OutputPort(static_cast<FwIndexType>(SEQUENCE_PORT.e),
                                        this->get_from_stepDispatchOut(static_cast<FwIndexType>(SEQUENCE_PORT.e)));
    this->clearHistory();
    bare.get_reportIn_InputPort(0)->invoke(FATAL);
    // Dispatch after every tick so that a long countdown does not overflow the (dropping) run queue
    for (FwSizeType i = 0; i < FaultManagerTester::TICKS_TO_RESPONSE; i++) {
        bare.get_run_InputPort(0)->invoke(0);
        for (FwSizeType j = 0; j < FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH; j++) {
            if (bare.m_queue.getMessagesAvailable() == 0) {
                break;
            }
            ASSERT_EQ(bare.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
        }
    }
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepPortUnconnected(0, REBOOT, REBOOT_PORT);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::FAULT);
    // With no countdown the FAULT_RESPONSE_FAILURE response (same step) has also run and failed by now
    const FwSizeType responses = (FaultConfig::RESPONSE_COUNTDOWN_TICKS == 0) ? 2 : 1;
    ASSERT_EVENTS_ResponseStarted_SIZE(responses);
    ASSERT_EVENTS_StepPortUnconnected_SIZE(responses);
    ASSERT_EVENTS_ResponseFailed_SIZE(responses);
    ASSERT_from_stepDispatchOut_SIZE(0);
    bare.deinit();
}

}  // namespace FaultProtection

}  // namespace Svc
