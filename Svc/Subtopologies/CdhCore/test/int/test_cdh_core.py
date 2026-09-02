"""test_cdh_core.py:

Test the core functionality of the CdhCore subtopology with:
1. Commands
2. Events
3. Telemetry channels
"""

import random
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def test_basic_command_and_event(fprime_test_api: IntegrationTestAPI):
    """Test that we can send a command and receive the expected event in response"""

    # Send NO_OP command to FSW and wait for expected event
    fprime_test_api.send_and_assert_command(
        f"{fprime_test_api.get_mnemonic('Svc.CommandDispatcher')}.CMD_NO_OP",
    )


def test_command_and_event_with_string_arg(fprime_test_api: IntegrationTestAPI):
    """Test that we can send a command with arguments and receive the expected event with args in response"""

    TEST_STRING = f"test string {random.random()}"

    test_event = fprime_test_api.get_event_pred("NoOpStringReceived", [TEST_STRING])

    # Send NO_OP command to FSW and wait for expected event
    fprime_test_api.send_and_assert_event(
        f"{fprime_test_api.get_mnemonic('Svc.CommandDispatcher')}.CMD_NO_OP_STRING",
        [TEST_STRING],
        events=[test_event],
        timeout=2,
    )


def test_command_and_event_with_many_args(fprime_test_api: IntegrationTestAPI):
    """Test that we can send a command with arguments and receive the expected event with args in response"""

    # types are (I32, F32, U8) - random float precision is finnicky, so just use a fixed value
    TEST_ARGS = [
        random.randint(-(2**31), 2**31 - 1),
        1.5,
        random.randint(0, 2**8 - 1),
    ]

    test_event = fprime_test_api.get_event_pred("TestCmd1Args", TEST_ARGS)

    # Send CMD_1 (no-op with args) command to FSW and wait for expected event
    fprime_test_api.send_and_assert_event(
        f"{fprime_test_api.get_mnemonic('Svc.CommandDispatcher')}.CMD_TEST_CMD_1",
        TEST_ARGS,
        events=[test_event],
        timeout=3,
    )


def test_telemetry_update(fprime_test_api: IntegrationTestAPI):
    """Test that we can receive telemetry updates with expected values"""

    cmd_dispatched_channel = fprime_test_api.get_telemetry_pred("CommandsDispatched")
    fprime_test_api.set_tlm_packet_level(3)
    fprime_test_api.clear_histories()

    # The SET_LEVEL dispatch above may still be in flight in the telemetry stream: wait until
    # two consecutive samples agree so the baseline reflects a settled counter.
    begin_result = fprime_test_api.await_telemetry(cmd_dispatched_channel, timeout=3)
    assert begin_result is not None, "No CommandsDispatched telemetry received"
    begin_tlm_val = begin_result.get_val()
    settled = fprime_test_api.await_telemetry(
        cmd_dispatched_channel, timeout=3, start="NOW"
    )
    assert settled is not None, "No CommandsDispatched telemetry received"
    begin_tlm_val = settled.get_val()

    # Send command and wait for completion with assert
    fprime_test_api.send_and_assert_command(
        f"{fprime_test_api.get_mnemonic('Svc.CommandDispatcher')}.CMD_NO_OP"
    )
    # Wait for a sample reflecting the dispatch (telemetry is emitted at the rate group period)
    end_result = fprime_test_api.await_telemetry(
        cmd_dispatched_channel, value=begin_tlm_val + 1, timeout=5, start="NOW"
    )
    assert (
        end_result is not None
    ), f"CommandsDispatched did not reach {begin_tlm_val + 1} after CMD_NO_OP"
