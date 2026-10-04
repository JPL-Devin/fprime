"""test_fault_protection.py:

Integration tests for the fault protection example in the Ref deployment: Ref.monitoredCounter counts rate group
cycles, its monitor reports the COUNTER_HIGH fault when the count stays above its threshold, and the fault manager
responds by running the RESET_COUNT_SEQUENCE and ACKNOWLEDGE_SEQUENCE sequences on the dedicated fpSeq sequencer.
The first resets the counter, correcting the fault.

The response sequences must be compiled and placed where the deployment expects them (see RefTopology.cpp):

    fprime-seqgen --dictionary <dictionary> Ref/sequences/RESET_COUNT_SEQUENCE.seq /tmp/fp-seq/RESET_COUNT_SEQUENCE.seq
    fprime-seqgen --dictionary <dictionary> Ref/sequences/ACKNOWLEDGE_SEQUENCE.seq /tmp/fp-seq/ACKNOWLEDGE_SEQUENCE.seq

Monitoring of the counter starts disabled (so a Ref without the sequences installed does not fault on its own); the
tests enable it for the session with SET_MONITORING and disable it again before the compiled sequences are removed.
The directory is kept short: CmdSequencer bounds the path to 40 characters.
"""

import json
import subprocess
from pathlib import Path

import pytest
from fprime_gds.common.testing_fw import predicates

SEQUENCE_DIRECTORY = Path("/tmp/fp-seq")
SEQUENCE_SOURCES = Path(__file__).parent.parent.parent / "sequences"
STEPS = ["RESET_COUNT_SEQUENCE", "ACKNOWLEDGE_SEQUENCE"]

# One excursion takes COUNT_THRESHOLD + SYSTEM_ERROR_THRESHOLD cycles (14 s at 1 Hz) plus the response countdown and
# two sequences; allow generous margin for a loaded test machine
EXCURSION_TIMEOUT = 60
# Cycles to wait past a report before concluding no response started: FaultConfig.RESPONSE_COUNTDOWN_TICKS (2) plus
# the tick that selects the response, with margin
COUNTDOWN_MARGIN_CYCLES = 5


@pytest.fixture(scope="session", autouse=True)
def response_sequences(fprime_test_api_session):
    """Compile the response sequences into the directory the deployment reads them from, removing them afterwards"""
    if SEQUENCE_DIRECTORY.is_symlink():
        pytest.fail(f"{SEQUENCE_DIRECTORY} is a symlink; refusing to write through it")
    SEQUENCE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    compiled = [SEQUENCE_DIRECTORY / f"{step}.seq" for step in STEPS]
    for step, target in zip(STEPS, compiled):
        subprocess.run(
            [
                "fprime-seqgen",
                "--dictionary",
                str(fprime_test_api_session.dictionaries.dictionary_path),
                str(SEQUENCE_SOURCES / f"{step}.seq"),
                str(target),
            ],
            check=True,
        )
    yield compiled
    for target in compiled:
        if target.exists():
            target.unlink()


@pytest.fixture(scope="session", autouse=True)
def monitoring_enabled(fprime_test_api_session, response_sequences):
    """Enable the counter monitor once the response sequences are in place; disable it before they are removed"""
    counter = fprime_test_api_session.get_mnemonic("Ref.MonitoredCounter")
    fprime_test_api_session.send_and_assert_command(f"{counter}.SET_MONITORING", ["ENABLED"], max_delay=5)
    fprime_test_api_session.clear_histories()
    yield
    fprime_test_api_session.send_and_assert_command(f"{counter}.SET_MONITORING", ["DISABLED"], max_delay=5)


def names(fprime_test_api):
    """Resolve the deployment instance names of the components under test"""
    return (
        fprime_test_api.get_mnemonic("Svc.FaultProtection.FaultManager"),
        fprime_test_api.get_mnemonic("Svc.FaultProtection.SequenceResponder"),
        fprime_test_api.get_mnemonic("Ref.MonitoredCounter"),
    )


def await_excursion(fprime_test_api):
    """Wait for one complete fault -> response -> correction excursion and return its events"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    fprime_test_api.clear_histories()
    return fprime_test_api.assert_event_sequence(
        [
            f"{COUNTER}.CountHighWarning",
            f"{COUNTER}.CountHighFault",
            f"{FAULT_MANAGER}.FaultReported",
            f"{FAULT_MANAGER}.ResponseStarted",
            f"{FAULT_MANAGER}.StepStarted",
            f"{SEQUENCE_RESPONDER}.SequenceStarted",
            f"{COUNTER}.CountReset",
            f"{SEQUENCE_RESPONDER}.SequenceCompleted",
            f"{FAULT_MANAGER}.StepCompleted",
            f"{FAULT_MANAGER}.StepStarted",
            f"{SEQUENCE_RESPONDER}.SequenceStarted",
            "CdhCore.cmdDisp.NoOpStringReceived",
            f"{SEQUENCE_RESPONDER}.SequenceCompleted",
            f"{FAULT_MANAGER}.StepCompleted",
            f"{FAULT_MANAGER}.ResponseCompleted",
        ],
        timeout=EXCURSION_TIMEOUT,
    )


def test_counter_fault_corrected_by_sequence(fprime_test_api):
    """The COUNTER_HIGH fault runs the two-step sequence response which resets the counter"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    results = await_excursion(fprime_test_api)
    reported = results[2]
    assert reported.get_args()[0].val == "COUNTER_HIGH"
    started = results[3]
    assert [arg.val for arg in started.get_args()] == ["RESET_COUNTER_RESPONSE", "COUNTER_HIGH"]
    first_step, second_step = results[4], results[9]
    assert first_step.get_args()[0].val == "RESET_COUNT_SEQUENCE"
    assert second_step.get_args()[0].val == "ACKNOWLEDGE_SEQUENCE"
    first_sequence = results[5]
    assert first_sequence.get_args()[2].val == str(SEQUENCE_DIRECTORY / "RESET_COUNT_SEQUENCE.seq")
    # The correction is visible in telemetry: the monitor recovers and the count restarts from the reset
    fprime_test_api.assert_telemetry(f"{COUNTER}.Monitor", value="GREEN", timeout=5)
    # Fault manager counters are written on response completion, which is within the excursion history
    fprime_test_api.assert_telemetry(f"{FAULT_MANAGER}.ResponsesFailed", value=0, timeout=5)
    fprime_test_api.clear_histories()
    count = fprime_test_api.assert_telemetry(f"{COUNTER}.Count", timeout=5)
    assert count.get_val() < 10


def test_fault_counters(fprime_test_api):
    """Fault manager telemetry counts reports and completed responses across excursions"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    await_excursion(fprime_test_api)
    reported = fprime_test_api.assert_telemetry(f"{FAULT_MANAGER}.FaultsReported", timeout=5).get_val()
    completed = fprime_test_api.assert_telemetry(f"{FAULT_MANAGER}.ResponsesCompleted", timeout=5).get_val()
    await_excursion(fprime_test_api)
    fprime_test_api.assert_telemetry(f"{FAULT_MANAGER}.FaultsReported", value=reported + 1, timeout=5)
    fprime_test_api.assert_telemetry(f"{FAULT_MANAGER}.ResponsesCompleted", value=completed + 1, timeout=5)


def test_disabled_fault_not_responded(fprime_test_api):
    """A disabled fault is acknowledged with FaultDisabled and no response runs"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.SET_FAULT_ENABLED", args=["COUNTER_HIGH", "DISABLED"], max_delay=5
    )
    fprime_test_api.assert_event(f"{FAULT_MANAGER}.FaultEnabledSet", timeout=5)
    try:
        fprime_test_api.clear_histories()
        fprime_test_api.assert_event_sequence(
            [f"{COUNTER}.CountHighFault", f"{FAULT_MANAGER}.FaultDisabled"], timeout=EXCURSION_TIMEOUT
        )
        # Outlast the response countdown (ticks of the same 1 Hz rate group that drives the counter) before
        # concluding that no response started
        count = fprime_test_api.await_telemetry(f"{COUNTER}.Count", timeout=5).get_val()
        fprime_test_api.assert_telemetry(f"{COUNTER}.Count", value=count + COUNTDOWN_MARGIN_CYCLES, timeout=15)
        fprime_test_api.assert_event_count(0, events=f"{FAULT_MANAGER}.ResponseStarted")
        # The counter stays uncorrected until commanded by the ground
        fprime_test_api.assert_telemetry(f"{COUNTER}.Monitor", value="RED", timeout=5)
        fprime_test_api.send_and_assert_command(f"{COUNTER}.RESET_COUNT", max_delay=5)
        fprime_test_api.assert_event(f"{COUNTER}.CountReset", timeout=5)
        fprime_test_api.assert_telemetry(f"{COUNTER}.Monitor", value="GREEN", timeout=5)
    finally:
        fprime_test_api.send_and_assert_command(
            f"{FAULT_MANAGER}.SET_FAULT_ENABLED", args=["COUNTER_HIGH", "ENABLED"], max_delay=5
        )
    # Protection is restored: the next excursion is corrected again
    await_excursion(fprime_test_api)


def test_disabled_response_skips_steps(fprime_test_api):
    """A disabled response walks its steps as skipped and completes without running sequences"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.SET_RESPONSE_ENABLED", args=["RESET_COUNTER_RESPONSE", "DISABLED"], max_delay=5
    )
    try:
        fprime_test_api.clear_histories()
        results = fprime_test_api.assert_event_sequence(
            [
                f"{COUNTER}.CountHighFault",
                f"{FAULT_MANAGER}.FaultReported",
                f"{FAULT_MANAGER}.ResponseStarted",
                f"{FAULT_MANAGER}.StepSkipped",
                f"{FAULT_MANAGER}.StepSkipped",
                f"{FAULT_MANAGER}.ResponseCompleted",
            ],
            timeout=EXCURSION_TIMEOUT,
        )
        assert [r.get_args()[0].val for r in results[3:5]] == STEPS
        fprime_test_api.assert_event_count(0, events=f"{SEQUENCE_RESPONDER}.SequenceStarted")
    finally:
        # The monitor reports once per excursion: the count must be reset by hand to arm the next excursion
        fprime_test_api.send_and_assert_command(
            f"{FAULT_MANAGER}.SET_RESPONSE_ENABLED", args=["RESET_COUNTER_RESPONSE", "ENABLED"], max_delay=5
        )
        fprime_test_api.send_and_assert_command(f"{COUNTER}.RESET_COUNT", max_delay=5)
    await_excursion(fprime_test_api)


def test_invalid_command_arguments_rejected(fprime_test_api):
    """Out-of-range enumeration values from the ground are rejected with a validation error, not an assertion"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    for command, args, event in [
        (f"{FAULT_MANAGER}.SET_FAULT_ENABLED", ["NUM_FAULTS", "DISABLED"], "InvalidFaultArgument"),
        (f"{FAULT_MANAGER}.SET_RESPONSE_ENABLED", ["NUM_RESPONSES", "DISABLED"], "InvalidResponseArgument"),
        (f"{FAULT_MANAGER}.UPDATE_STEP_FAILURE_MODE", ["SKIP", "IGNORE"], "InvalidStepArgument"),
    ]:
        fprime_test_api.clear_histories()
        fprime_test_api.send_command(command, args=args)
        fprime_test_api.assert_event(f"{FAULT_MANAGER}.{event}", timeout=5)
        fprime_test_api.assert_event("CdhCore.cmdDisp.OpCodeError", timeout=5)
    # The system is still protected
    await_excursion(fprime_test_api)


# ----------------------------------------------------------------------
# Variants: parameters, sequencer off-nominal paths
# ----------------------------------------------------------------------

TEST_SEQUENCES = Path(__file__).parent
FAILURE_FAULT = "FAULT_RESPONSE_FAILURE"


def compile_sequence(fprime_test_api, source, destination):
    """Compile a sequence source with fprime-seqgen into the given destination"""
    result = subprocess.run(
        ["fprime-seqgen", "--dictionary", str(fprime_test_api.dictionaries.dictionary_path), str(source), str(destination)]
    )
    assert result.returncode == 0, f"Failed to compile {source}"


@pytest.fixture(scope="session")
def test_sequences(fprime_test_api_session):
    """Compile the off-nominal test sequences next to the response sequences"""
    compiled = {}
    for name in ["wait_sequence", "failing_sequence"]:
        compiled[name] = SEQUENCE_DIRECTORY / f"{name}.seq"
        compile_sequence(fprime_test_api_session, TEST_SEQUENCES / f"{name}.seq", compiled[name])
    return compiled


@pytest.fixture
def replaced_step(request, fprime_test_api, test_sequences):
    """Replace a response step's sequence file for one test, restoring it (and the counter) afterwards

    Parametrize indirectly with (step, replacement) where replacement is a compiled test sequence name or None to
    remove the file.
    """
    step, replacement = request.param
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    target = SEQUENCE_DIRECTORY / f"{step}.seq"
    backup = SEQUENCE_DIRECTORY / f"{step}.seq.bak"
    target.rename(backup)
    if replacement is not None:
        target.write_bytes(test_sequences[replacement].read_bytes())
    try:
        yield step
    finally:
        if target.exists():
            target.unlink()
        backup.rename(target)
        fprime_test_api.send_and_assert_command(f"{COUNTER}.RESET_COUNT", max_delay=5)


@pytest.fixture
def failure_fault_disabled(fprime_test_api):
    """Disable the FAULT_RESPONSE_FAILURE fault so that a failed response does not escalate to a reboot"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.SET_FAULT_ENABLED", args=[FAILURE_FAULT, "DISABLED"], max_delay=5
    )
    try:
        yield
    finally:
        fprime_test_api.send_and_assert_command(
            f"{FAULT_MANAGER}.SET_FAULT_ENABLED", args=[FAILURE_FAULT, "ENABLED"], max_delay=5
        )


def assert_command_error(fprime_test_api, command, args, error):
    """Send a command expected to fail and check the dispatcher's error response"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(command, args=args)
    event = fprime_test_api.assert_event("CdhCore.cmdDisp.OpCodeError", timeout=5)
    assert event.get_args()[1].val == error


def await_failed_response(fprime_test_api, step):
    """Wait for a response that fails at the given step because its sequence fails, with escalation disabled"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    results = fprime_test_api.assert_event_sequence(
        [
            f"{COUNTER}.CountHighFault",
            f"{FAULT_MANAGER}.FaultReported",
            f"{FAULT_MANAGER}.ResponseStarted",
            f"{FAULT_MANAGER}.StepStarted",
            f"{SEQUENCE_RESPONDER}.SequenceStarted",
            f"{SEQUENCE_RESPONDER}.SequenceFailed",
            f"{FAULT_MANAGER}.StepFailed",
            f"{FAULT_MANAGER}.ResponseFailed",
            f"{FAULT_MANAGER}.FaultDisabled",
        ],
        timeout=EXCURSION_TIMEOUT,
    )
    assert [arg.val for arg in results[6].get_args()] == [step, "RESET_COUNTER_RESPONSE", "COUNTER_HIGH", "FAULT"]
    assert results[8].get_args()[0].val == FAILURE_FAULT
    fprime_test_api.assert_event_count(0, events=f"{FAULT_MANAGER}.ResponseCompleted")
    return results


@pytest.fixture
def remove_saved_parameters(request):
    """Remove the PrmDb file written into the deployment's bin directory by PRM_SAVE_FILE

    A stray file beside the deployment binary makes `fprime-gds -d <deployment>` refuse to select the application.
    """
    yield
    deployment = request.config.getoption("--deployment")
    if deployment:
        saved = Path(deployment) / "bin" / "PrmDb.dat"
        if saved.exists():
            saved.unlink()


def test_parameter_persistence(fprime_test_api, remove_saved_parameters):
    """The tables modified by command can be saved to the parameter database and set from the ground"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    PRM_DB = fprime_test_api.get_mnemonic("Svc.PrmDb")
    # RESPONSE_TABLE: command, save, persist to file
    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.SET_RESPONSE_ENABLED", args=["RESET_COUNTER_RESPONSE", "DISABLED"], max_delay=5
    )
    try:
        fprime_test_api.send_and_assert_command(f"{FAULT_MANAGER}.RESPONSE_TABLE_PRM_SAVE", max_delay=5)
        fprime_test_api.clear_histories()
        fprime_test_api.send_and_assert_command(f"{PRM_DB}.PRM_SAVE_FILE", max_delay=5)
        fprime_test_api.assert_event(f"{PRM_DB}.PrmFileSaveComplete", timeout=5)
    finally:
        fprime_test_api.send_and_assert_command(
            f"{FAULT_MANAGER}.SET_RESPONSE_ENABLED", args=["RESET_COUNTER_RESPONSE", "ENABLED"], max_delay=5
        )
        fprime_test_api.send_and_assert_command(f"{FAULT_MANAGER}.RESPONSE_TABLE_PRM_SAVE", max_delay=5)
    # STEP_TABLE: command then save
    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.UPDATE_STEP_FAILURE_MODE", args=["ACKNOWLEDGE_SEQUENCE", "DEFER"], max_delay=5
    )
    try:
        fprime_test_api.send_and_assert_command(f"{FAULT_MANAGER}.STEP_TABLE_PRM_SAVE", max_delay=5)
    finally:
        fprime_test_api.send_and_assert_command(
            f"{FAULT_MANAGER}.UPDATE_STEP_FAILURE_MODE", args=["ACKNOWLEDGE_SEQUENCE", "IGNORE"], max_delay=5
        )
        fprime_test_api.send_and_assert_command(f"{FAULT_MANAGER}.STEP_TABLE_PRM_SAVE", max_delay=5)
    # FAULT_RESPONSE_TABLE: PRM_SET of a table disabling the counter fault takes effect immediately
    def table(counter_enabled):
        return json.dumps(
            [
                {"fault": "FATAL_OCCURRED", "precedence": 10, "response": "REBOOT_RESPONSE", "enabled": "ENABLED"},
                {"fault": FAILURE_FAULT, "precedence": 20, "response": "REBOOT_RESPONSE", "enabled": "ENABLED"},
                {"fault": "COUNTER_HIGH", "precedence": 5, "response": "RESET_COUNTER_RESPONSE", "enabled": counter_enabled},
            ]
        )

    fprime_test_api.send_and_assert_command(
        f"{FAULT_MANAGER}.FAULT_RESPONSE_TABLE_PRM_SET", args=[table("DISABLED")], max_delay=5
    )
    try:
        fprime_test_api.send_and_assert_command(f"{FAULT_MANAGER}.FAULT_RESPONSE_TABLE_PRM_SAVE", max_delay=5)
        fprime_test_api.clear_histories()
        fprime_test_api.assert_event_sequence(
            [f"{COUNTER}.CountHighFault", f"{FAULT_MANAGER}.FaultDisabled"], timeout=EXCURSION_TIMEOUT
        )
        fprime_test_api.assert_event_count(0, events=f"{FAULT_MANAGER}.ResponseStarted")
        # Invalid tables are rejected and leave the active table (counter fault disabled) in place
        bad = json.loads(table("DISABLED"))
        bad[2]["response"] = "NUM_RESPONSES"
        assert_command_error(
            fprime_test_api, f"{FAULT_MANAGER}.FAULT_RESPONSE_TABLE_PRM_SET", [json.dumps(bad)], "VALIDATION_ERROR"
        )
        bad = json.loads(table("DISABLED"))
        bad[0]["fault"] = "NUM_FAULTS"
        assert_command_error(
            fprime_test_api, f"{FAULT_MANAGER}.FAULT_RESPONSE_TABLE_PRM_SET", [json.dumps(bad)], "VALIDATION_ERROR"
        )
        fprime_test_api.send_and_assert_command(f"{COUNTER}.RESET_COUNT", max_delay=5)
        fprime_test_api.clear_histories()
        fprime_test_api.assert_event_sequence(
            [f"{COUNTER}.CountHighFault", f"{FAULT_MANAGER}.FaultDisabled"], timeout=EXCURSION_TIMEOUT
        )
    finally:
        fprime_test_api.send_and_assert_command(
            f"{FAULT_MANAGER}.FAULT_RESPONSE_TABLE_PRM_SET", args=[table("ENABLED")], max_delay=5
        )
        fprime_test_api.send_and_assert_command(f"{FAULT_MANAGER}.FAULT_RESPONSE_TABLE_PRM_SAVE", max_delay=5)
        fprime_test_api.send_and_assert_command(f"{PRM_DB}.PRM_SAVE_FILE", max_delay=5)
        fprime_test_api.send_and_assert_command(f"{COUNTER}.RESET_COUNT", max_delay=5)
    await_excursion(fprime_test_api)


@pytest.mark.parametrize("replaced_step", [("ACKNOWLEDGE_SEQUENCE", None)], indirect=True)
def test_missing_sequence_ignored_step(fprime_test_api, replaced_step):
    """A missing sequence file fails its step; with failure mode IGNORE the response still completes"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    fprime_test_api.clear_histories()
    results = fprime_test_api.assert_event_sequence(
        [
            f"{COUNTER}.CountHighFault",
            f"{FAULT_MANAGER}.ResponseStarted",
            f"{COUNTER}.CountReset",
            f"{FAULT_MANAGER}.StepCompleted",
            f"{FAULT_MANAGER}.StepStarted",
            f"{SEQUENCE_RESPONDER}.SequenceStarted",
            f"{SEQUENCE_RESPONDER}.SequenceFailed",
            f"{FAULT_MANAGER}.StepFailed",
            f"{FAULT_MANAGER}.ResponseCompleted",
        ],
        timeout=EXCURSION_TIMEOUT,
    )
    assert [arg.val for arg in results[7].get_args()] == [replaced_step, "RESET_COUNTER_RESPONSE", "COUNTER_HIGH", "IGNORE"]
    fprime_test_api.assert_event_count(0, events=f"{FAULT_MANAGER}.ResponseFailed")
    fprime_test_api.assert_telemetry(f"{FAULT_MANAGER}.ResponsesFailed", value=0, timeout=5)


@pytest.mark.parametrize("replaced_step", [("RESET_COUNT_SEQUENCE", None)], indirect=True)
def test_missing_sequence_fault_step(fprime_test_api, failure_fault_disabled, replaced_step):
    """A missing sequence file at a FAULT step fails the response, which reports FAULT_RESPONSE_FAILURE"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    fprime_test_api.clear_histories()
    await_failed_response(fprime_test_api, replaced_step)
    fprime_test_api.assert_telemetry(
        f"{FAULT_MANAGER}.ResponsesFailed", value=predicates.greater_than(0), timeout=5
    )
    # The fault was not corrected: the monitor stays red until the ground resets the counter (fixture teardown)
    fprime_test_api.assert_telemetry(f"{COUNTER}.Monitor", value="RED", timeout=5)


@pytest.mark.parametrize("replaced_step", [("ACKNOWLEDGE_SEQUENCE", "failing_sequence")], indirect=True)
def test_sequence_fails_mid_way(fprime_test_api, replaced_step):
    """A sequence whose command is rejected fails the step; the rest of the sequence does not run"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    FAULT_SEQUENCER = fprime_test_api.get_mnemonic("Ref.FaultSequencer")
    fprime_test_api.clear_histories()
    results = fprime_test_api.assert_event_sequence(
        [
            f"{COUNTER}.CountHighFault",
            f"{COUNTER}.CountReset",
            f"{FAULT_MANAGER}.StepStarted",
            f"{SEQUENCE_RESPONDER}.SequenceStarted",
            "CdhCore.cmdDisp.NoOpReceived",
            f"{FAULT_MANAGER}.InvalidFaultArgument",
            f"{FAULT_SEQUENCER}.CS_CommandError",
            f"{SEQUENCE_RESPONDER}.SequenceFailed",
            f"{FAULT_MANAGER}.StepFailed",
            f"{FAULT_MANAGER}.ResponseCompleted",
        ],
        timeout=EXCURSION_TIMEOUT,
    )
    assert results[8].get_args()[0].val == replaced_step
    fprime_test_api.assert_event_count(0, events="CdhCore.cmdDisp.NoOpStringReceived")


@pytest.mark.parametrize("replaced_step", [("RESET_COUNT_SEQUENCE", "wait_sequence")], indirect=True)
def test_cancel_mid_sequence(fprime_test_api, failure_fault_disabled, replaced_step):
    """Cancelling the fault sequencer while a step's sequence runs fails the step"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    FAULT_SEQUENCER = fprime_test_api.get_mnemonic("Ref.FaultSequencer")
    fprime_test_api.clear_histories()
    fprime_test_api.assert_event_sequence(
        [f"{COUNTER}.CountHighFault", f"{FAULT_MANAGER}.StepStarted", f"{SEQUENCE_RESPONDER}.SequenceStarted"],
        timeout=EXCURSION_TIMEOUT,
    )
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(f"{FAULT_SEQUENCER}.CS_CANCEL", max_delay=5)
    results = fprime_test_api.assert_event_sequence(
        [
            f"{SEQUENCE_RESPONDER}.SequenceFailed",
            f"{FAULT_SEQUENCER}.CS_SequenceCanceled",
            f"{FAULT_MANAGER}.StepFailed",
            f"{FAULT_MANAGER}.ResponseFailed",
            f"{FAULT_MANAGER}.FaultDisabled",
        ],
        timeout=10,
    )
    assert results[4].get_args()[0].val == FAILURE_FAULT


def test_sequencer_busy_with_ground_sequence(fprime_test_api, failure_fault_disabled, test_sequences):
    """A step dispatched while the fault sequencer runs a ground sequence fails without disturbing that sequence"""
    FAULT_MANAGER, SEQUENCE_RESPONDER, COUNTER = names(fprime_test_api)
    FAULT_SEQUENCER = fprime_test_api.get_mnemonic("Ref.FaultSequencer")
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{FAULT_SEQUENCER}.CS_RUN", args=[str(test_sequences["wait_sequence"]), "NO_BLOCK"], max_delay=5
    )
    fprime_test_api.assert_event(f"{FAULT_SEQUENCER}.CS_SequenceLoaded", timeout=5)
    try:
        fprime_test_api.clear_histories()
        await_failed_response(fprime_test_api, "RESET_COUNT_SEQUENCE")
        fprime_test_api.assert_event(f"{FAULT_SEQUENCER}.CS_InvalidMode", timeout=5)
        fprime_test_api.assert_event_count(0, events=f"{FAULT_SEQUENCER}.CS_SequenceCanceled")
    finally:
        # End the ground sequence; its completion status reaches the responder, which has no active step
        fprime_test_api.clear_histories()
        fprime_test_api.send_and_assert_command(f"{FAULT_SEQUENCER}.CS_CANCEL", max_delay=5)
        fprime_test_api.assert_event(f"{SEQUENCE_RESPONDER}.UnexpectedSequenceDone", timeout=5)
        fprime_test_api.send_and_assert_command(f"{COUNTER}.RESET_COUNT", max_delay=5)
    await_excursion(fprime_test_api)
