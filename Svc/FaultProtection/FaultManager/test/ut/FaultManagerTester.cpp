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
      m_complete_on_dispatch(false),
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
    ASSERT_EVENTS_FaultInvalid_SIZE(1);
    ASSERT_EVENTS_FaultInvalid(0, static_cast<U8>(FaultConfig::Fault::NUM_FAULTS));
    // Nothing was queued for the component thread
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), 0);
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
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Fault::NUM_FAULTS));
    ASSERT_EVENTS_FaultEnabledSet_SIZE(0);
    this->sendCommandSetResponseEnabled(badResponse, Fw::Enabled::DISABLED, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Response::NUM_RESPONSES));
    ASSERT_EVENTS_ResponseEnabledSet_SIZE(0);
    this->sendCommandUpdateStepFailureMode(badStep, FaultConfig::FailureMode::IGNORE,
                                           Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Step::SKIP));
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

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void FaultManagerTester ::from_stepDispatchOut_handler(FwIndexType portNum,
                                                       const FaultConfig::Response& response,
                                                       const FaultConfig::Step& step,
                                                       const FaultConfig::Context& context) {
    this->m_last_dispatch_port = portNum;
    this->pushFromPortEntry_stepDispatchOut(response, step, context);
    if (this->m_complete_on_dispatch) {
        // Responder completes before the dispatch call returns (e.g. a synchronous responder)
        this->invoke_to_stepCompletionIn(0, Fw::Success(Fw::Success::SUCCESS), response, step);
    }
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

void FaultManagerTester ::dispatchAll() {
    // Bounded: each dispatched message queues at most one further message and a response has finitely many steps
    for (FwSizeType i = 0; i < TEST_INSTANCE_QUEUE_DEPTH; i++) {
        if (this->component.m_queue.getMessagesAvailable() == 0) {
            return;
        }
        ASSERT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    }
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), 0);
}

void FaultManagerTester ::report(const FaultConfig::Fault& fault) {
    this->invoke_to_reportIn(0, fault);
    this->dispatchAll();
}

void FaultManagerTester ::tick(FwSizeType count) {
    for (FwSizeType i = 0; i < count; i++) {
        this->invoke_to_run(0, 0);
        this->dispatchAll();
    }
}

void FaultManagerTester ::complete(const Fw::Success& status,
                                   const FaultConfig::Response& response,
                                   const FaultConfig::Step& step) {
    this->invoke_to_stepCompletionIn(0, status, response, step);
    this->dispatchAll();
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
    FaultResponseTable table = this->component.m_fault_parameter;
    for (FwSizeType i = 0; i < FaultResponseTable::SIZE; i++) {
        if (table[i].get_fault() == fault) {
            table[i].set_response(response);
            table[i].set_precedence(precedence);
        }
    }
    this->paramSet_FAULT_RESPONSE_TABLE(table, Fw::ParamValid::VALID);
    this->component.loadParameters();
    this->clearHistory();
}

void FaultManagerTester ::sendCommandSetFaultEnabled(const FaultConfig::Fault& fault,
                                                     const Fw::Enabled& enabled,
                                                     const Fw::CmdResponse& expected) {
    this->clearHistory();
    this->sendCmd_SET_FAULT_ENABLED(0, 1, fault, enabled);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_SET_FAULT_ENABLED, 1, expected);
}

void FaultManagerTester ::sendCommandSetResponseEnabled(const FaultConfig::Response& response,
                                                        const Fw::Enabled& enabled,
                                                        const Fw::CmdResponse& expected) {
    this->clearHistory();
    this->sendCmd_SET_RESPONSE_ENABLED(0, 2, response, enabled);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_SET_RESPONSE_ENABLED, 2, expected);
}

void FaultManagerTester ::sendCommandUpdateStepFailureMode(const FaultConfig::Step& step,
                                                           const FaultConfig::FailureMode& mode,
                                                           const Fw::CmdResponse& expected) {
    this->clearHistory();
    this->sendCmd_UPDATE_STEP_FAILURE_MODE(0, 3, step, mode);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_UPDATE_STEP_FAILURE_MODE, 3, expected);
}

// ----------------------------------------------------------------------
// Variant test support
// ----------------------------------------------------------------------

namespace {

//! Port a framework-default step dispatches through
FaultConfig::Port portOf(const FaultConfig::Step& step) {
    return (step == FaultConfig::Step::REBOOT) ? FaultConfig::Port(FaultConfig::Port::REBOOT_RESPONDER_PORT)
                                               : FaultConfig::Port(FaultConfig::Port::SEQUENCE_RESPONDER_PORT);
}

//! Step placed at slot `index` of a full-length response: alternating RUN_SEQUENCE / REBOOT
FaultConfig::Step fullLengthStep(FwSizeType index) {
    return ((index % 2) == 0) ? FaultConfig::Step(FaultConfig::Step::RUN_SEQUENCE)
                              : FaultConfig::Step(FaultConfig::Step::REBOOT);
}

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

const FwSizeType STEP_COUNT = FaultConfig::FAULT_RESPONSE_STEP_COUNT;
const FaultConfig::Step SKIP(FaultConfig::Step::SKIP);

}  // namespace

void FaultManagerTester ::defineResponse(const FaultConfig::Response& response,
                                         const FaultConfig::Step* steps,
                                         FwSizeType count) {
    ASSERT_LE(count, STEP_COUNT);
    const FwSizeType index = this->component.responseToResponseEntryIndex(response);
    ASSERT_NE(index, static_cast<FwSizeType>(FaultManager::NO_ACTIVE_INDEX));
    Steps table(SKIP);
    for (FwSizeType i = 0; i < count; i++) {
        table[i] = steps[i];
    }
    this->component.m_response_definition_table[index].set_steps(table);
}

void FaultManagerTester ::setStepPort(const FaultConfig::Step& step, const FaultConfig::Port& port) {
    const FwSizeType index = this->component.stepToStepEntryIndex(step);
    ASSERT_NE(index, static_cast<FwSizeType>(FaultManager::NO_ACTIVE_INDEX));
    this->component.m_step_definition_table[index].set_dispatchPort(port);
}

void FaultManagerTester ::setPrecedence(const FaultConfig::Fault& fault, U8 precedence) {
    const FwSizeType index = this->component.faultToFaultEntryIndex(fault);
    ASSERT_NE(index, static_cast<FwSizeType>(FaultManager::NO_ACTIVE_INDEX));
    this->remapFault(fault, this->component.m_fault_parameter[index].get_response(), precedence);
}

void FaultManagerTester ::fillQueue() {
    for (FwSizeType i = 0; i < FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH; i++) {
        this->invoke_to_run(0, 0);
    }
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(),
              static_cast<FwSizeType>(FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH));
}

void FaultManagerTester ::drainQueue() {
    // Each dispatched run tick may queue one state machine signal: bound at twice the depth plus slack
    for (FwSizeType i = 0; i < 4 * FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH; i++) {
        if (this->component.m_queue.getMessagesAvailable() == 0) {
            return;
        }
        ASSERT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    }
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(), 0u);
}

void FaultManagerTester ::drainResponse() {
    for (FwSizeType i = 0; i <= STEP_COUNT; i++) {
        if (this->eventHistory_ResponseCompleted->size() > 0) {
            return;
        }
        ASSERT_EQ(this->fromPortHistory_stepDispatchOut->size(), 1u) << "no step dispatched to complete";
        const FromPortEntry_stepDispatchOut entry = this->fromPortHistory_stepDispatchOut->at(0);
        this->clearFromPortHistory();
        this->complete(Fw::Success::SUCCESS, entry.response, entry.step);
    }
    ASSERT_EQ(this->eventHistory_ResponseCompleted->size(), 1u);
}

void FaultManagerTester ::runFullLengthResponse(const FaultConfig::Fault& fault,
                                                FwSizeType failIndex,
                                                const FaultConfig::FailureMode& mode) {
    FaultConfig::Step steps[FaultConfig::FAULT_RESPONSE_STEP_COUNT];
    for (FwSizeType i = 0; i < STEP_COUNT; i++) {
        steps[i] = fullLengthStep(i);
    }
    this->defineResponse(SEQUENCE_RESPONSE, steps, STEP_COUNT);
    this->remapFault(fault, SEQUENCE_RESPONSE, 10);
    if (failIndex < STEP_COUNT) {
        this->sendCommandUpdateStepFailureMode(steps[failIndex], mode, Fw::CmdResponse::OK);
    }
    this->clearHistory();
    this->report(fault);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(1);
    ASSERT_EVENTS_ResponseStarted(0, SEQUENCE_RESPONSE, fault);

    bool failed = false;
    for (FwSizeType i = 0; i < STEP_COUNT; i++) {
        ASSERT_EVENTS_StepStarted_SIZE(1);
        ASSERT_EVENTS_StepStarted(0, steps[i], SEQUENCE_RESPONSE, fault);
        this->assertDispatched(portOf(steps[i]), SEQUENCE_RESPONSE, steps[i]);
        this->clearHistory();
        if (i == failIndex) {
            this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, steps[i]);
            ASSERT_EVENTS_StepFailed_SIZE(1);
            ASSERT_EVENTS_StepFailed(0, steps[i], SEQUENCE_RESPONSE, fault, mode);
            ASSERT_EVENTS_StepCompleted_SIZE(0);
            failed = true;
            if (mode == FaultConfig::FailureMode::FAULT) {
                ASSERT_EVENTS_ResponseFailed_SIZE(1);
                ASSERT_EVENTS_ResponseFailed(0, SEQUENCE_RESPONSE, fault);
                ASSERT_EVENTS_FaultReported_SIZE(1);
                ASSERT_EVENTS_FaultReported(0, FAILURE);
                if (FaultConfig::RESPONSE_COUNTDOWN_TICKS > 0) {
                    // The escalation waits out the countdown (with none configured it starts at once)
                    this->assertNotDispatched();
                    ASSERT_EVENTS_StepStarted_SIZE(0);
                }
                break;
            }
        } else {
            this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, steps[i]);
            ASSERT_EVENTS_StepCompleted_SIZE(1);
            ASSERT_EVENTS_StepCompleted(0, steps[i], SEQUENCE_RESPONSE, fault);
        }
        if (i + 1 < STEP_COUNT) {
            // Response continues with the next step
            ASSERT_EVENTS_ResponseCompleted_SIZE(0);
            ASSERT_EVENTS_ResponseFailed_SIZE(0);
        }
    }
    const bool escalated = failed && (mode != FaultConfig::FailureMode::IGNORE);
    if (escalated) {
        ASSERT_EVENTS_ResponseFailed_SIZE(1);
        ASSERT_EVENTS_ResponseCompleted_SIZE(0);
        ASSERT_EVENTS_FaultReported_SIZE(1);
        ASSERT_EVENTS_FaultReported(0, FAILURE);
        // FAULT_RESPONSE_FAILURE is handled by the (default) reboot response
        this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
        this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
        ASSERT_EVENTS_ResponseCompleted_SIZE(1);
        ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
    } else {
        ASSERT_EVENTS_ResponseCompleted_SIZE(1);
        ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, fault);
        ASSERT_EVENTS_ResponseFailed_SIZE(0);
        ASSERT_EVENTS_FaultReported_SIZE(0);
    }
    this->clearHistory();
    // Back to idle: nothing else happens
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_SIZE(0);
    this->assertNotDispatched();
}

// ----------------------------------------------------------------------
// Variant tests: multi-step responses
// ----------------------------------------------------------------------

void FaultManagerTester ::testTwoStepResponse() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    const FaultConfig::Step steps[] = {RUN_SEQUENCE, REBOOT};
    this->defineResponse(SEQUENCE_RESPONSE, steps, 2);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    // Second step waits for the first to complete, regardless of ticks
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    this->assertNotDispatched();
    ASSERT_EVENTS_StepStarted_SIZE(0);
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_StepCompleted_SIZE(1);
    ASSERT_EVENTS_StepCompleted(0, RUN_SEQUENCE, SEQUENCE_RESPONSE, FATAL);
    ASSERT_EVENTS_StepStarted_SIZE(1);
    ASSERT_EVENTS_StepStarted(0, REBOOT, SEQUENCE_RESPONSE, FATAL);
    this->assertDispatched(REBOOT_PORT, SEQUENCE_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepCompleted_SIZE(1);
    ASSERT_EVENTS_StepSkipped_SIZE(0);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    ASSERT_TLM_ResponsesCompleted_SIZE(1);
    ASSERT_TLM_ResponsesCompleted(0, 1);
    this->assertNotDispatched();
}

void FaultManagerTester ::testFullLengthResponse() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    this->runFullLengthResponse(FATAL, STEP_COUNT, FaultConfig::FailureMode::FAULT);
    ASSERT_EQ(this->component.m_responses_completed, 1u);
    // Run it again: a reused step definition is not consumed
    this->runFullLengthResponse(FATAL, STEP_COUNT, FaultConfig::FailureMode::FAULT);
    ASSERT_EQ(this->component.m_responses_completed, 2u);
}

void FaultManagerTester ::testSharedStepAcrossResponses() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    // REBOOT is the first step of REBOOT_RESPONSE (default) and the second step of SEQUENCE_RESPONSE
    const FaultConfig::Step steps[] = {RUN_SEQUENCE, REBOOT};
    this->defineResponse(SEQUENCE_RESPONSE, steps, 2);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->assertDispatched(REBOOT_PORT, SEQUENCE_RESPONSE, REBOOT);
    this->clearHistory();
    // Completion of the shared step attributed to the other response is rejected
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(1);
    ASSERT_EVENTS_UnexpectedStepCompleted(0, REBOOT, REBOOT_RESPONSE);
    ASSERT_EVENTS_StepCompleted_SIZE(0);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    this->clearHistory();
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    this->clearHistory();
    // The other response still dispatches the shared step through the same port
    this->reportAndDispatch(FAILURE, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
    this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
}

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

void FaultManagerTester ::testFailureModeMatrix() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    const FwSizeType positions[] = {0, STEP_COUNT / 2, STEP_COUNT - 1};
    const FaultConfig::FailureMode modes[] = {FaultConfig::FailureMode(FaultConfig::FailureMode::IGNORE),
                                              FaultConfig::FailureMode(FaultConfig::FailureMode::DEFER),
                                              FaultConfig::FailureMode(FaultConfig::FailureMode::FAULT)};
    for (FwSizeType p = 0; p < FW_NUM_ARRAY_ELEMENTS(positions); p++) {
        for (FwSizeType m = 0; m < FW_NUM_ARRAY_ELEMENTS(modes); m++) {
            SCOPED_TRACE(::testing::Message() << "fail position " << positions[p] << " mode " << modes[m].e);
            this->runFullLengthResponse(FATAL, positions[p], modes[m]);
            if (::testing::Test::HasFailure()) {
                return;
            }
        }
    }
}

void FaultManagerTester ::testDeferThenFault() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    FaultConfig::Step steps[FaultConfig::FAULT_RESPONSE_STEP_COUNT];
    for (FwSizeType i = 0; i < STEP_COUNT; i++) {
        steps[i] = fullLengthStep(i);
    }
    this->defineResponse(SEQUENCE_RESPONSE, steps, STEP_COUNT);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->sendCommandUpdateStepFailureMode(RUN_SEQUENCE, FaultConfig::FailureMode::DEFER, Fw::CmdResponse::OK);
    this->sendCommandUpdateStepFailureMode(REBOOT, FaultConfig::FailureMode::FAULT, Fw::CmdResponse::OK);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_StepFailed_SIZE(1);
    ASSERT_EVENTS_StepFailed(0, RUN_SEQUENCE, SEQUENCE_RESPONSE, FATAL, FaultConfig::FailureMode::DEFER);
    ASSERT_EVENTS_ResponseFailed_SIZE(0);
    this->assertDispatched(REBOOT_PORT, SEQUENCE_RESPONSE, REBOOT);
    this->clearHistory();
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed_SIZE(1);
    ASSERT_EVENTS_StepFailed(0, REBOOT, SEQUENCE_RESPONSE, FATAL, FaultConfig::FailureMode::FAULT);
    // Exactly one response failure, one escalation
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
    ASSERT_TLM_ResponsesFailed(this->tlmHistory_ResponsesFailed->size() - 1, 1);
    // The response stopped at the FAULT step; the escalation is responded to next
    this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
}

void FaultManagerTester ::testDeferThenIgnore() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    const FaultConfig::Step steps[] = {RUN_SEQUENCE, REBOOT};
    this->defineResponse(SEQUENCE_RESPONSE, steps, 2);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->sendCommandUpdateStepFailureMode(RUN_SEQUENCE, FaultConfig::FailureMode::DEFER, Fw::CmdResponse::OK);
    this->sendCommandUpdateStepFailureMode(REBOOT, FaultConfig::FailureMode::IGNORE, Fw::CmdResponse::OK);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->assertDispatched(REBOOT_PORT, SEQUENCE_RESPONSE, REBOOT);
    this->clearHistory();
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, REBOOT);
    ASSERT_EVENTS_StepFailed(0, REBOOT, SEQUENCE_RESPONSE, FATAL, FaultConfig::FailureMode::IGNORE);
    // The deferred failure is remembered through the ignored one
    ASSERT_EVENTS_ResponseFailed_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted_SIZE(0);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
}

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
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
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

void FaultManagerTester ::testPreemptAtEachStep() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    FaultConfig::Step steps[FaultConfig::FAULT_RESPONSE_STEP_COUNT];
    for (FwSizeType i = 0; i < STEP_COUNT; i++) {
        steps[i] = fullLengthStep(i);
    }
    this->defineResponse(SEQUENCE_RESPONSE, steps, STEP_COUNT);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    for (FwSizeType preempt_at = 0; preempt_at < STEP_COUNT; preempt_at++) {
        SCOPED_TRACE(::testing::Message() << "preempt at step index " << preempt_at);
        this->clearHistory();
        this->reportAndDispatch(FATAL, portOf(steps[0]), SEQUENCE_RESPONSE, steps[0]);
        for (FwSizeType i = 0; i < preempt_at; i++) {
            this->clearHistory();
            this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, steps[i]);
            this->assertDispatched(portOf(steps[i + 1]), SEQUENCE_RESPONSE, steps[i + 1]);
        }
        this->clearHistory();
        this->report(FAILURE);
        ASSERT_EVENTS_StepCancel_SIZE(1);
        ASSERT_EVENTS_StepCancel(0, steps[preempt_at]);
        ASSERT_from_stepCancelOut_SIZE(1);
        ASSERT_EQ(this->m_last_cancel_port, static_cast<FwIndexType>(portOf(steps[preempt_at]).e));
        ASSERT_EVENTS_ResponsePreempted_SIZE(1);
        ASSERT_EVENTS_ResponsePreempted(0, SEQUENCE_RESPONSE, FATAL, FAILURE);
        ASSERT_EVENTS_ResponseCompleted_SIZE(0);
        if (FaultConfig::RESPONSE_COUNTDOWN_TICKS > 0) {
            // The preempting response waits out the countdown (with none configured it starts at once)
            this->assertNotDispatched();
        }
        // Late completion of the cancelled step is ignored
        this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, steps[preempt_at]);
        ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(1);
        this->awaitPendingResponse(REBOOT_RESPONSE, FAILURE, REBOOT_PORT, REBOOT);
        this->complete(Fw::Success::SUCCESS, REBOOT_RESPONSE, REBOOT);
        ASSERT_EVENTS_ResponseCompleted(0, REBOOT_RESPONSE, FAILURE);
        // The preempted fault was kept latched: it is responded to afterwards, from its first step
        this->awaitPendingResponse(SEQUENCE_RESPONSE, FATAL, portOf(steps[0]), steps[0]);
        this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, steps[0]);
        this->drainResponse();
        ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
        if (::testing::Test::HasFailure()) {
            return;
        }
    }
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

void FaultManagerTester ::testCompletionDuringDispatch() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    const FaultConfig::Step steps[] = {RUN_SEQUENCE, REBOOT};
    this->defineResponse(SEQUENCE_RESPONSE, steps, 2);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->m_complete_on_dispatch = true;
    this->report(FATAL);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_from_stepDispatchOut_SIZE(2);
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->at(0).response, SEQUENCE_RESPONSE);
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->at(0).step, RUN_SEQUENCE);
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->at(1).response, SEQUENCE_RESPONSE);
    ASSERT_EQ(this->fromPortHistory_stepDispatchOut->at(1).step, REBOOT);
    ASSERT_EVENTS_UnexpectedStepCompleted_SIZE(0);
    ASSERT_EVENTS_StepCompleted_SIZE(2);
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    this->m_complete_on_dispatch = false;
}

void FaultManagerTester ::testQueueFullReportLatched() {
    this->fillQueue();
    // Queue is full: the report's announcement is dropped but the latch is set synchronously
    this->invoke_to_reportIn(0, FATAL);
    ASSERT_TRUE(this->component.m_sm_state.latched_fault_reports[FATAL.e]);
    ASSERT_EQ(this->component.m_queue.getMessagesAvailable(),
              static_cast<FwSizeType>(FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH));
    this->drainQueue();
    ASSERT_EVENTS_FaultReported_SIZE(0);  // the announcement was lost
    // The queued ticks ran the countdown down; a long countdown needs the remaining ticks
    if (FaultManagerTester::TICKS_TO_RESPONSE > FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH) {
        this->tick(FaultManagerTester::TICKS_TO_RESPONSE - FaultManagerTester::TEST_INSTANCE_QUEUE_DEPTH);
    }
    ASSERT_EVENTS_ResponseStarted_SIZE(1);  // the response was not lost
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    this->assertDispatched(REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
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

void FaultManagerTester ::testDisableFaultDuringCountdown() {
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS < 1) {
        GTEST_SKIP() << "requires a non-zero RESPONSE_COUNTDOWN_TICKS";
    }
    this->report(FATAL);
    this->tick(1);
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(0);
    this->assertNotDispatched();
    // Re-enable: the stale latch was cleared, a fresh report responds normally
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted_SIZE(0);
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

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

void FaultManagerTester ::testDisableResponseMidResponse() {
    if (STEP_COUNT < 2) {
        GTEST_SKIP() << "requires FAULT_RESPONSE_STEP_COUNT >= 2";
    }
    FaultConfig::Step steps[FaultConfig::FAULT_RESPONSE_STEP_COUNT];
    for (FwSizeType i = 0; i < STEP_COUNT; i++) {
        steps[i] = fullLengthStep(i);
    }
    this->defineResponse(SEQUENCE_RESPONSE, steps, STEP_COUNT);
    this->remapFault(FATAL, SEQUENCE_RESPONSE, 10);
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->sendCommandSetResponseEnabled(SEQUENCE_RESPONSE, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    // The active step is not cancelled
    ASSERT_from_stepCancelOut_SIZE(0);
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_SIZE(0);
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_StepCompleted_SIZE(1);
    // Remaining steps are skipped, none dispatched
    ASSERT_EVENTS_StepSkipped_SIZE(STEP_COUNT - 1);
    this->assertNotDispatched();
    ASSERT_EVENTS_ResponseCompleted_SIZE(1);
    ASSERT_EVENTS_ResponseCompleted(0, SEQUENCE_RESPONSE, FATAL);
    // Re-enabled: the full response runs again
    this->sendCommandSetResponseEnabled(SEQUENCE_RESPONSE, Fw::Enabled::ENABLED, Fw::CmdResponse::OK);
    this->clearHistory();
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::SUCCESS, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->drainResponse();
    ASSERT_EVENTS_StepSkipped_SIZE(0);
    ASSERT_EVENTS_StepCompleted_SIZE(STEP_COUNT);
}

void FaultManagerTester ::sendRawCommand(FwOpcodeType opcode,
                                         U32 cmdSeq,
                                         Fw::CmdArgBuffer& args,
                                         const Fw::CmdResponse& expected) {
    this->clearHistory();
    Fw::InputCmdPort* const port = this->component.get_cmdIn_InputPort(0);
    ASSERT_NE(port, nullptr);
    port->invoke(static_cast<FwOpcodeType>(this->component.getIdBase() + opcode), cmdSeq, args);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, opcode, cmdSeq, expected);
}

void FaultManagerTester ::testParameterPersistence() {
    // The generated tester base asserts that each saved value equals the expected value set here
    FaultResponseTable expected_faults = this->component.m_fault_parameter;
    ResponsesEnabled expected_responses = this->component.m_response_parameter;
    StepFailureModes expected_steps = this->component.m_step_parameter;

    // FAULT_RESPONSE_TABLE: commanded change is persisted by PRM_SAVE
    this->sendCommandSetFaultEnabled(FATAL, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    const FwSizeType fatal_index = this->component.faultToFaultEntryIndex(FATAL);
    ASSERT_NE(fatal_index, static_cast<FwSizeType>(FaultManager::NO_ACTIVE_INDEX));
    expected_faults[fatal_index].set_enabled(Fw::Enabled::DISABLED);
    this->paramSet_FAULT_RESPONSE_TABLE(expected_faults, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSave_FAULT_RESPONSE_TABLE(0, 10);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SAVE, 10, Fw::CmdResponse::OK);
    ASSERT_EQ(this->paramTesterDelegate.m_param_FAULT_RESPONSE_TABLE_valid, Fw::ParamValid::VALID);

    // RESPONSE_TABLE: commanded change is persisted by PRM_SAVE
    this->sendCommandSetResponseEnabled(SEQUENCE_RESPONSE, Fw::Enabled::DISABLED, Fw::CmdResponse::OK);
    expected_responses[SEQUENCE_RESPONSE.e] = Fw::Enabled::DISABLED;
    this->paramSet_RESPONSE_TABLE(expected_responses, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSave_RESPONSE_TABLE(0, 11);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_RESPONSE_TABLE_SAVE, 11, Fw::CmdResponse::OK);
    ASSERT_EQ(this->paramTesterDelegate.m_param_RESPONSE_TABLE_valid, Fw::ParamValid::VALID);

    // STEP_TABLE: the parameter default makes PRM_SAVE acceptable before any PRM_SET (regression: without the
    // default the parameter was never VALID/DEFAULT and PRM_SAVE was refused after a commanded change)
    this->sendCommandUpdateStepFailureMode(REBOOT, FaultConfig::FailureMode::IGNORE, Fw::CmdResponse::OK);
    expected_steps[REBOOT.e] = FaultConfig::FailureMode::IGNORE;
    this->paramSet_STEP_TABLE(expected_steps, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSave_STEP_TABLE(0, 12);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_STEP_TABLE_SAVE, 12, Fw::CmdResponse::OK);
    ASSERT_EQ(this->paramTesterDelegate.m_param_STEP_TABLE_valid, Fw::ParamValid::VALID);
    ASSERT_EQ(this->paramTesterDelegate.m_param_STEP_TABLE, expected_steps);

    // PRM_SET paths: ground-set tables take effect
    ResponsesEnabled responses(Fw::Enabled(Fw::Enabled::ENABLED));
    this->paramSet_RESPONSE_TABLE(responses, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_RESPONSE_TABLE(0, 13);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_RESPONSE_TABLE_SET, 13, Fw::CmdResponse::OK);
    StepFailureModes modes(FaultConfig::FailureMode(FaultConfig::FailureMode::DEFER));
    this->paramSet_STEP_TABLE(modes, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_STEP_TABLE(0, 14);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_STEP_TABLE_SET, 14, Fw::CmdResponse::OK);
    FaultResponseTable faults = this->component.m_fault_parameter;
    faults[0].set_enabled(Fw::Enabled::ENABLED);
    faults[0].set_response(SEQUENCE_RESPONSE);
    this->paramSet_FAULT_RESPONSE_TABLE(faults, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, 15);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, 15, Fw::CmdResponse::OK);
    this->clearHistory();
    // Effect: FATAL re-enabled and mapped to the (re-enabled) sequence response, whose step failure now defers
    this->reportAndDispatch(FATAL, SEQUENCE_PORT, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    this->complete(Fw::Success::FAILURE, SEQUENCE_RESPONSE, RUN_SEQUENCE);
    ASSERT_EVENTS_StepFailed(0, RUN_SEQUENCE, SEQUENCE_RESPONSE, FATAL, FaultConfig::FailureMode::DEFER);
}

void FaultManagerTester ::testParameterValidation() {
    const FaultResponseTable original = this->component.m_fault_parameter;
    U32 seq = 20;

    // Response id out of range
    FaultResponseTable table = original;
    table[0].set_response(FaultConfig::Response(FaultConfig::Response::NUM_RESPONSES));
    this->paramSet_FAULT_RESPONSE_TABLE(table, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, seq);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, seq++, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EQ(this->component.m_fault_parameter, original);

    // Fault id out of range
    table = original;
    table[1].set_fault(FaultConfig::Fault(FaultConfig::Fault::NUM_FAULTS));
    this->paramSet_FAULT_RESPONSE_TABLE(table, Fw::ParamValid::VALID);
    this->clearHistory();
    this->paramSend_FAULT_RESPONSE_TABLE(0, seq);
    this->dispatchAll();
    ASSERT_CMD_RESPONSE(0, FaultManager::OPCODE_FAULT_RESPONSE_TABLE_SET, seq++, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EQ(this->component.m_fault_parameter, original);

    // Invalid enabled value in RESPONSE_TABLE (rejected by enumeration deserialization)
    {
        Fw::CmdArgBuffer raw;
        for (FwSizeType k = 0; k < ResponsesEnabled::SIZE; k++) {
            ASSERT_EQ(raw.serializeFrom(static_cast<U8>((k == 0) ? 7 : Fw::Enabled::ENABLED)), Fw::FW_SERIALIZE_OK);
        }
        this->sendRawCommand(FaultManager::OPCODE_RESPONSE_TABLE_SET, seq++, raw, Fw::CmdResponse::VALIDATION_ERROR);
        ASSERT_EQ(this->component.m_response_parameter[0], Fw::Enabled::ENABLED);
    }

    // Invalid failure mode in STEP_TABLE (rejected by enumeration deserialization)
    {
        Fw::CmdArgBuffer raw;
        for (FwSizeType k = 0; k < StepFailureModes::SIZE; k++) {
            ASSERT_EQ(raw.serializeFrom(static_cast<I32>((k == 0) ? 9 : FaultConfig::FailureMode::FAULT)),
                      Fw::FW_SERIALIZE_OK);
        }
        this->sendRawCommand(FaultManager::OPCODE_STEP_TABLE_SET, seq++, raw, Fw::CmdResponse::VALIDATION_ERROR);
        ASSERT_EQ(this->component.m_step_parameter[0], FaultConfig::FailureMode::FAULT);
    }

    // Short buffer
    {
        Fw::CmdArgBuffer raw;
        ASSERT_EQ(raw.serializeFrom(static_cast<U8>(Fw::Enabled::ENABLED)), Fw::FW_SERIALIZE_OK);
        this->sendRawCommand(FaultManager::OPCODE_RESPONSE_TABLE_SET, seq++, raw, Fw::CmdResponse::VALIDATION_ERROR);
    }

    // Behavior is unchanged
    this->clearHistory();
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

    // NUM_* sentinels (and SKIP) are enumerators but not configured items: validated by the handlers
    this->sendCommandSetFaultEnabled(FaultConfig::Fault(FaultConfig::Fault::NUM_FAULTS), Fw::Enabled::DISABLED,
                                     Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Fault::NUM_FAULTS));
    this->sendCommandSetResponseEnabled(FaultConfig::Response(FaultConfig::Response::NUM_RESPONSES),
                                        Fw::Enabled::DISABLED, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Response::NUM_RESPONSES));
    this->sendCommandUpdateStepFailureMode(FaultConfig::Step(FaultConfig::Step::NUM_STEPS),
                                           FaultConfig::FailureMode::IGNORE, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Step::NUM_STEPS));
    this->sendCommandUpdateStepFailureMode(SKIP, FaultConfig::FailureMode::IGNORE, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidCommandArgument_SIZE(1);
    ASSERT_EVENTS_InvalidCommandArgument(0, static_cast<U8>(FaultConfig::Step::SKIP));

    // Nothing changed
    ASSERT_EQ(this->component.m_fault_parameter[0].get_enabled(), Fw::Enabled::ENABLED);
    ASSERT_EQ(this->component.m_response_parameter[REBOOT_RESPONSE.e], Fw::Enabled::ENABLED);
    ASSERT_EQ(this->component.m_step_parameter[REBOOT.e], FaultConfig::FailureMode::FAULT);
    this->clearHistory();
    this->reportAndDispatch(FATAL, REBOOT_PORT, REBOOT_RESPONSE, REBOOT);
}

void FaultManagerTester ::testStepPortNumPorts() {
    this->setStepPort(REBOOT, FaultConfig::Port(FaultConfig::Port::NUM_PORTS));
    this->report(FATAL);
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_ResponseStarted(0, REBOOT_RESPONSE, FATAL);
    ASSERT_EVENTS_StepPortUnconnected(0, REBOOT, FaultConfig::Port::NUM_PORTS);
    ASSERT_EVENTS_StepFailed(0, REBOOT, REBOOT_RESPONSE, FATAL, FaultConfig::FailureMode::FAULT);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    ASSERT_EVENTS_FaultReported(0, FAILURE);
    this->assertNotDispatched();
    // The failure response uses the same step: it fails too, without re-reporting
    if (FaultConfig::RESPONSE_COUNTDOWN_TICKS > 0) {
        // With a countdown, only the first response has run so far
        ASSERT_EVENTS_ResponseStarted_SIZE(1);
        ASSERT_EVENTS_StepPortUnconnected_SIZE(1);
        ASSERT_EVENTS_StepFailed_SIZE(1);
        ASSERT_EVENTS_ResponseFailed_SIZE(1);
        this->tick(FaultConfig::RESPONSE_COUNTDOWN_TICKS);
    }
    ASSERT_EVENTS_ResponseStarted_SIZE(2);
    ASSERT_EVENTS_ResponseStarted(1, REBOOT_RESPONSE, FAILURE);
    ASSERT_EVENTS_StepPortUnconnected_SIZE(2);
    ASSERT_EVENTS_ResponseFailed_SIZE(2);
    ASSERT_EVENTS_FaultReported_SIZE(1);
    this->assertNotDispatched();
    this->clearHistory();
    this->tick(FaultManagerTester::TICKS_TO_RESPONSE);
    ASSERT_EVENTS_SIZE(0);
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
