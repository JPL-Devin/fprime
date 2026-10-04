"""test_fault_protection_fatal.py:

End-to-end FATAL path of the fault protection example in the Ref deployment. Each scenario ends the Ref process (the
RebootResponder exits it, or the FatalToFault fallback aborts it), so exactly one scenario runs per deployment
launch, selected with the FP_FATAL_SCENARIO environment variable:

    reboot   (default) FATAL -> FATAL_OCCURRED -> REBOOT_RESPONSE -> RebootResponder exits the process
    preempt  FATAL during the lower-precedence COUNTER_HIGH response: the response is preempted, the running
             sequence cancelled, and the REBOOT_RESPONSE exits the process
    fallback FATAL with the FATAL_OCCURRED fault disabled: no response runs and the FatalToFault fallback aborts the
             process after its countdown

The FATAL is produced by Svc.Health: the ping thresholds of Ref.pingRcvr are lowered and its ping replies stopped.
Run against a GDS started with the Ref deployment, e.g.:

    FP_FATAL_SCENARIO=preempt pytest test_fault_protection_fatal.py --deployment <...> --dictionary <...> \\
        --deployment-config int_config.json
"""

import os
import subprocess
import time
from pathlib import Path

import pytest

from fprime_gds.common.testing_fw import predicates

from test_fault_protection import SEQUENCE_DIRECTORY, SEQUENCE_SOURCES, STEPS, EXCURSION_TIMEOUT, compile_sequence

SCENARIO = os.environ.get("FP_FATAL_SCENARIO", "reboot")
PING_ENTRY = "Ref_pingRcvr"
# Health pings on the 1/4 Hz rate group: warn after one missed ping, FATAL after two (about 8-12 s)
PING_WARN, PING_FATAL = 1, 2
FATAL_TIMEOUT = 60
# FatalToFault fallback: 5 ticks of the 1/4 Hz rate group after the FATAL
FALLBACK_TIMEOUT = 45
# RebootResponder::doReboot ends the process with _Exit() in the tick that dispatches the REBOOT step, so the events of
# that tick (ResponseStarted, StepStarted, RebootRequested) are still queued in the ActiveLogger and may never downlink.
# The process exit is the deterministic evidence of the REBOOT_RESPONSE; its events are checked when they arrive.
REBOOT_EXIT_TIMEOUT = 10


def process_running():
    return subprocess.run(["pgrep", "-f", "build-artifacts/Linux/Ref/bin/Ref"], capture_output=True).returncode == 0


def await_process_exit(timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if not process_running():
            return True
        time.sleep(1)
    return False


def assert_reboot_response_exits(fprime_test_api, after):
    """The process ends within REBOOT_EXIT_TIMEOUT; FaultManager events downlinked after `after` are REBOOT_RESPONSE's"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, REBOOT_RESPONDER, HEALTH, PING_RECEIVER, COUNTER = names(fprime_test_api)
    assert await_process_exit(REBOOT_EXIT_TIMEOUT), "RebootResponder did not end the Ref process"
    later = predicates.greater_than(after.get_time())
    started = fprime_test_api.get_event_pred(f"{FAULT_MANAGER}.ResponseStarted", time_pred=later)
    step_started = fprime_test_api.get_event_pred(f"{FAULT_MANAGER}.StepStarted", time_pred=later)
    for event in fprime_test_api.get_event_test_history().retrieve():
        if started(event):
            assert [arg.val for arg in event.get_args()] == ["REBOOT_RESPONSE", "FATAL_OCCURRED"]
        if step_started(event):
            assert event.get_args()[0].val == "REBOOT"


def names(fprime_test_api):
    return (
        fprime_test_api.get_mnemonic("Svc.FaultProtection.FaultManager"),
        fprime_test_api.get_mnemonic("Svc.FaultProtection.SequenceResponder"),
        fprime_test_api.get_mnemonic("Svc.FaultProtection.RebootResponder"),
        fprime_test_api.get_mnemonic("Svc.Health"),
        fprime_test_api.get_mnemonic("Ref.PingReceiver"),
        fprime_test_api.get_mnemonic("Ref.MonitoredCounter"),
    )


@pytest.fixture(scope="session", autouse=True)
def response_sequences(fprime_test_api_session):
    SEQUENCE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    for step in STEPS:
        compile_sequence(fprime_test_api_session, SEQUENCE_SOURCES / f"{step}.seq", SEQUENCE_DIRECTORY / f"{step}.seq")
    yield
    # Restore the response sequence that the preempt scenario replaces with a waiting sequence
    backup = SEQUENCE_DIRECTORY / "RESET_COUNT_SEQUENCE.seq.bak"
    if backup.exists():
        backup.replace(SEQUENCE_DIRECTORY / "RESET_COUNT_SEQUENCE.seq")


def trigger_fatal(fprime_test_api):
    """Lower the ping thresholds of the ping receiver and stop its replies: Svc.Health raises HLTH_PING_LATE (FATAL)"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, REBOOT_RESPONDER, HEALTH, PING_RECEIVER, COUNTER = names(fprime_test_api)
    fprime_test_api.send_and_assert_command(
        f"{HEALTH}.HLTH_CHNG_PING", args=[PING_ENTRY, PING_WARN, PING_FATAL], max_delay=5
    )
    fprime_test_api.assert_event(f"{HEALTH}.HLTH_PING_UPDATED", timeout=5)
    fprime_test_api.clear_histories()
    # PingReceiver declares PR_PingsDisabled but its PR_StopPings handler never emits it; rely on the command response
    fprime_test_api.send_and_assert_command(f"{PING_RECEIVER}.PR_StopPings", max_delay=5)


@pytest.mark.skipif(SCENARIO != "reboot", reason="FP_FATAL_SCENARIO selects another scenario")
def test_fatal_reboots(fprime_test_api):
    """A FATAL event is reported as FATAL_OCCURRED; the REBOOT_RESPONSE ends the process"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, REBOOT_RESPONDER, HEALTH, PING_RECEIVER, COUNTER = names(fprime_test_api)
    assert process_running()
    trigger_fatal(fprime_test_api)
    results = fprime_test_api.assert_event_sequence(
        [
            f"{HEALTH}.HLTH_PING_WARN",
            f"{HEALTH}.HLTH_PING_LATE",
            f"{FAULT_MANAGER}.FaultReported",
        ],
        timeout=FATAL_TIMEOUT,
    )
    assert results[2].get_args()[0].val == "FATAL_OCCURRED"
    assert_reboot_response_exits(fprime_test_api, after=results[2])


@pytest.mark.skipif(SCENARIO != "preempt", reason="FP_FATAL_SCENARIO selects another scenario")
def test_fatal_preempts_active_response(fprime_test_api):
    """A FATAL during the lower-precedence counter response preempts it, cancels its sequence, and reboots"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, REBOOT_RESPONDER, HEALTH, PING_RECEIVER, COUNTER = names(fprime_test_api)
    FAULT_SEQUENCER = fprime_test_api.get_mnemonic("Ref.FaultSequencer")
    assert process_running()
    # Hold the counter response in its first step with a waiting sequence
    target = SEQUENCE_DIRECTORY / "RESET_COUNT_SEQUENCE.seq"
    target.rename(SEQUENCE_DIRECTORY / "RESET_COUNT_SEQUENCE.seq.bak")
    compile_sequence(fprime_test_api, Path(__file__).parent / "wait_sequence.seq", target)
    # The counter monitor starts disabled: enable it so that COUNTER_HIGH is reported
    fprime_test_api.send_and_assert_command(f"{COUNTER}.SET_MONITORING", ["ENABLED"], max_delay=5)
    fprime_test_api.clear_histories()
    fprime_test_api.assert_event_sequence(
        [f"{COUNTER}.CountHighFault", f"{FAULT_MANAGER}.ResponseStarted", f"{SEQUENCE_RESPONDER}.SequenceStarted"],
        timeout=EXCURSION_TIMEOUT,
    )
    trigger_fatal(fprime_test_api)
    results = fprime_test_api.assert_event_sequence(
        [
            f"{HEALTH}.HLTH_PING_LATE",
            f"{FAULT_MANAGER}.FaultReported",
            # the cancel is requested (and the responder reports the step cancelled) before ResponsePreempted
            f"{FAULT_MANAGER}.StepCancel",
            f"{SEQUENCE_RESPONDER}.SequenceCanceled",
            f"{FAULT_MANAGER}.ResponsePreempted",
            f"{FAULT_SEQUENCER}.CS_SequenceCanceled",
        ],
        timeout=FATAL_TIMEOUT,
    )
    assert results[1].get_args()[0].val == "FATAL_OCCURRED"
    assert results[2].get_args()[0].val == "RESET_COUNT_SEQUENCE"
    assert [arg.val for arg in results[4].get_args()] == ["RESET_COUNTER_RESPONSE", "COUNTER_HIGH", "FATAL_OCCURRED"]
    assert_reboot_response_exits(fprime_test_api, after=results[4])
    # The cancelled sequence's completion is not a step completion
    fprime_test_api.assert_event_count(0, events=f"{FAULT_MANAGER}.StepCompleted")


@pytest.mark.skipif(SCENARIO != "fallback", reason="FP_FATAL_SCENARIO selects another scenario")
def test_fatal_fallback_when_fault_disabled(fprime_test_api):
    """With FATAL_OCCURRED disabled no response runs; the FatalToFault fallback ends the process after its countdown"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, REBOOT_RESPONDER, HEALTH, PING_RECEIVER, COUNTER = names(fprime_test_api)
    assert process_running()
    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.SET_FAULT_ENABLED", args=["FATAL_OCCURRED", "DISABLED"], max_delay=5
    )
    trigger_fatal(fprime_test_api)
    results = fprime_test_api.assert_event_sequence(
        [f"{HEALTH}.HLTH_PING_LATE", f"{FAULT_MANAGER}.FaultDisabled"], timeout=FATAL_TIMEOUT
    )
    assert results[1].get_args()[0].val == "FATAL_OCCURRED"
    fatal_time = time.time()
    reboot_started = fprime_test_api.get_event_pred(f"{FAULT_MANAGER}.ResponseStarted", args=["REBOOT_RESPONSE", None])
    fprime_test_api.assert_event_count(0, events=reboot_started)
    assert await_process_exit(FALLBACK_TIMEOUT), "FatalToFault fallback did not end the Ref process"
    # The fallback waited for its countdown rather than aborting immediately
    assert time.time() - fatal_time > 5
    fprime_test_api.assert_event_count(0, events=f"{REBOOT_RESPONDER}.RebootRequested")
