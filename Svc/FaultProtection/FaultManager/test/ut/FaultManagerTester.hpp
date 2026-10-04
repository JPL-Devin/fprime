// ======================================================================
// \title  FaultManagerTester.hpp
// \author mstarch
// \brief  hpp file for FaultManager component test harness implementation class
// ======================================================================

#ifndef Svc_FaultProtection_FaultManagerTester_HPP
#define Svc_FaultProtection_FaultManagerTester_HPP

#include "Svc/FaultProtection/FaultManager/FaultManager.hpp"
#include "Svc/FaultProtection/FaultManager/FaultManagerGTestBase.hpp"

namespace Svc {

namespace FaultProtection {

class FaultManagerTester final : public FaultManagerGTestBase {
  public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 60;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Queue depth supplied to the component instance under test
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

    //! Default configuration: the number of ticks from an idle report until the first step is dispatched
    static const FwSizeType TICKS_TO_RESPONSE = FaultConfig::RESPONSE_COUNTDOWN_TICKS + 1;

  public:
    FaultManagerTester();
    ~FaultManagerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Ticks with no report leave the manager idle and silent
    void testIdle();

    //! A report is latched, counted down, responded to, and cleared on success
    void testNominalResponse();

    //! A duplicate report of a latched fault is ignored
    void testDuplicateReportIgnored();

    //! An invalid fault id is rejected
    void testInvalidReport();

    //! A disabled fault is reported but not responded to; re-enabling restores the response
    void testFaultDisabled();

    //! A failed step with failure mode FAULT fails the response and latches FAULT_RESPONSE_FAILURE
    void testStepFailureFault();

    //! A failed step with failure mode IGNORE is treated as success
    void testStepFailureIgnore();

    //! A failed step with failure mode DEFER completes the response then fails it
    void testStepFailureDefer();

    //! A disabled response skips its steps and clears the latch
    void testResponseDisabled();

    //! A higher-precedence report preempts the active response; the preempted fault is responded to afterwards
    void testPreemption();

    //! A lower-precedence report during a response waits for the active response to complete
    void testNoPreemptionLowerPrecedence();

    //! When two faults are latched during the countdown the higher precedence response is selected first
    void testPrecedenceSelection();

    //! Completions not matching the active step are rejected
    void testUnexpectedCompletion();

    //! Commands reject enumeration values outside the configured ranges
    void testCommandValidation();

    //! A failed response to FAULT_RESPONSE_FAILURE does not escalate (no recursion)
    void testResponseFailureNoRecursion();
    void testMultiStepResponse();
    void testDeferThenContinue();
    void testStepTimeout();
    void testStepPortUnconnected();
    void testIgnoredReportThrottle();
    void testDisableClearsLatch();
    void testTableParameters();

    // ----------------------------------------------------------------------
    // Variant tests: multi-step responses
    // ----------------------------------------------------------------------

    //! A two-step response dispatches its second step only after the first completes
    void testTwoStepResponse();

    //! A response using every step slot, reusing a step, dispatches each step in order
    void testFullLengthResponse();

    //! A step shared by two responses completes only for the response that dispatched it
    void testSharedStepAcrossResponses();

    //! Two faults mapped to one response: one response clears both latches
    void testSharedResponseAcrossFaults();

    // ----------------------------------------------------------------------
    // Variant tests: failure modes
    // ----------------------------------------------------------------------

    //! Every failure mode at the first, middle, and last step of a full-length response
    void testFailureModeMatrix();

    //! A deferred failure followed by a FAULT failure fails the response once
    void testDeferThenFault();

    //! A deferred failure followed by an ignored failure still fails the response at its end
    void testDeferThenIgnore();

    // ----------------------------------------------------------------------
    // Variant tests: precedence and preemption
    // ----------------------------------------------------------------------

    //! Equal-precedence faults latched during the countdown: the earliest table entry wins
    void testEqualPrecedenceCountdown();

    //! An equal-precedence report during a response does not preempt
    void testEqualPrecedenceNoPreempt();

    //! A report latched during a response starts its response at the completion (zero countdown) or after the countdown
    void testPendingReportAfterResponse();

    //! A lower-precedence report during the countdown waits for the higher-precedence response
    void testLowerPrecedenceDuringCountdown();

    //! A second report during the countdown does not restart the countdown
    void testCountdownNotRestarted();

    //! A higher-precedence report preempts at each step of a full-length response, cancelling that step
    void testPreemptAtEachStep();

    //! Three faults of mixed precedence reported in various orders are responded to in precedence order
    void testThreeFaultChain();

    // ----------------------------------------------------------------------
    // Variant tests: latching and timing
    // ----------------------------------------------------------------------

    //! A reporter flapping every tick is bounded to one FaultReported and throttled FaultIgnored events
    void testFlappingReporter();

    //! A completion delivered while the dispatch is still on the stack is accepted
    void testCompletionDuringDispatch();

    //! A report arriving while the queue is full is latched and responded to
    void testQueueFullReportLatched();

    //! A state machine signal sent while the queue is full asserts (documents SVC_FAULTMANAGER_018 gap)
    void testQueueFullSignalAsserts();

    // ----------------------------------------------------------------------
    // Variant tests: enable/disable
    // ----------------------------------------------------------------------

    //! Disabling a fault during the countdown drops the pending response
    void testDisableFaultDuringCountdown();

    //! Disabling a fault during its response lets the response finish; new reports are then ignored
    void testDisableFaultDuringResponse();

    //! Disabling a response mid-response skips the remaining steps; re-enabling resumes dispatch
    void testDisableResponseMidResponse();

    //! Parameter persistence: PRM_SAVE of every table after the commands that alter them
    void testParameterPersistence();

    //! Invalid parameter values are rejected and leave the active tables untouched
    void testParameterValidation();

    // ----------------------------------------------------------------------
    // Variant tests: input validation
    // ----------------------------------------------------------------------

    //! Commands with out-of-range values beyond the sentinels are rejected
    void testCommandInvalidEnumerations();

    //! A step whose dispatch port is NUM_PORTS is treated as failed
    void testStepPortNumPorts();

    //! A step whose dispatch port is not connected is treated as failed
    void testUnconnectedDispatchPort();

    //! A report whose handleReport message is dropped from a full queue is still responded to on the next ticks
    void testReportDroppedFromQueue();

    //! PRM_SAVE of the never-set RESPONSE_TABLE and STEP_TABLE persists the active tables
    void testParameterSave();

    //! A higher-precedence report whose handleReport message is dropped still preempts on the next tick
    void testPreemptionAfterDroppedReport();

    //! A disabled fault's report whose handleReport message is dropped is discarded on the next tick
    void testDisabledReportDroppedFromQueue();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    void from_stepDispatchOut_handler(FwIndexType portNum,
                                      const FaultConfig::Response& response,
                                      const FaultConfig::Step& step,
                                      const FaultConfig::Context& context) override;

    void from_stepCancelOut_handler(FwIndexType portNum) override;

    //! FaultReported hook: optionally fill the component queue to stage a full queue
    void logIn_ACTIVITY_HI_FaultReported(const FaultConfig::Fault& fault) override;

    //! Print text events to aid debugging
    void textLogIn(FwEventIdType id,
                   const Fw::Time& timeTag,
                   const Fw::LogSeverity severity,
                   const Fw::TextLogString& text) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Dispatch every message queued on the component (ports, commands, and state machine signals)
    void dispatchAll(FaultManager& target);

    //! Report a fault and dispatch the internal report handling
    void report(const FaultConfig::Fault& fault);

    //! Tick the manager `count` times
    void tick(FwSizeType count = 1);

    //! Fill the component queue with run ticks (without dispatching) so that further messages are dropped
    void fillQueue();

    //! Dispatch the filled queue to empty, bounded
    void drainQueue();

    //! Complete the active step with the given status and dispatch
    void complete(const Fw::Success& status, const FaultConfig::Response& response, const FaultConfig::Step& step);

    //! Assert a single dispatch on the given port of the given response/step, then clear the history
    void assertDispatched(const FaultConfig::Port& port,
                          const FaultConfig::Response& response,
                          const FaultConfig::Step& step);

    //! Assert nothing has been dispatched
    void assertNotDispatched();

    //! A completion, failure, or preemption that leaves a report latched starts that response after
    //! RESPONSE_COUNTDOWN_TICKS ticks (on the completing dispatch itself when zero): wait for it, assert the most
    //! recent ResponseStarted is the given one and that exactly its first step was dispatched, then clear the history
    //! Tick enough for a full countdown while a step is active, staying short of the step's timeout
    void tickWithinTimeout(const FaultConfig::Step& step);

    void awaitPendingResponse(const FaultConfig::Response& response,
                              const FaultConfig::Fault& fault,
                              const FaultConfig::Port& port,
                              const FaultConfig::Step& step);

    //! Drive a report through countdown to its first dispatched step
    void reportAndDispatch(const FaultConfig::Fault& fault,
                           const FaultConfig::Port& port,
                           const FaultConfig::Response& response,
                           const FaultConfig::Step& step);

    //! Remap a fault's response via the FAULT_RESPONSE_TABLE parameter
    void remapFault(const FaultConfig::Fault& fault, const FaultConfig::Response& response, U8 precedence);

    //! Send a command and dispatch it, asserting the given response
    void sendCommandSetFaultEnabled(const FaultConfig::Fault& fault,
                                    const Fw::Enabled& enabled,
                                    const Fw::CmdResponse& expected);
    void sendCommandSetResponseEnabled(const FaultConfig::Response& response,
                                       const Fw::Enabled& enabled,
                                       const Fw::CmdResponse& expected);
    void sendCommandUpdateStepFailureMode(const FaultConfig::Step& step,
                                          const FaultConfig::FailureMode& mode,
                                          const Fw::CmdResponse& expected);

    //! Redefine a response's steps (unused slots are filled with SKIP)
    void defineResponse(const FaultConfig::Response& response, const FaultConfig::Step* steps, FwSizeType count);

    //! Redefine a step's dispatch port
    void setStepPort(const FaultConfig::Step& step, const FaultConfig::Port& port);

    //! Set a fault's precedence via the FAULT_RESPONSE_TABLE parameter
    void setPrecedence(const FaultConfig::Fault& fault, U8 precedence);

    //! Drive a full-length response through step `count` completions, failing step `failIndex` (or none)
    void runFullLengthResponse(const FaultConfig::Fault& fault,
                               FwSizeType failIndex,
                               const FaultConfig::FailureMode& mode);

    //! Send a command built from a raw argument buffer (used to inject malformed enumeration values)
    void sendRawCommand(FwOpcodeType opcode, U32 cmdSeq, Fw::CmdArgBuffer& args, const Fw::CmdResponse& expected);

    //! Complete whatever steps are dispatched, in order, until the active response completes
    void drainResponse();

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

  private:
    //! The component under test
    FaultManager component;

    //! Port number of the most recent step dispatch
    FwIndexType m_last_dispatch_port;

    //! Port number of the most recent step cancel
    FwIndexType m_last_cancel_port;

    //! When set, step dispatches are completed synchronously from within the dispatch handler
    bool m_complete_on_dispatch;

    //! When set, the FaultReported event handler fills the component queue
    bool m_fill_queue_on_report;
};

}  // namespace FaultProtection

}  // namespace Svc

#endif
