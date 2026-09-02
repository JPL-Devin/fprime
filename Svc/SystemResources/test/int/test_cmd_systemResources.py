"""test_cmd_systemResources.py:

Test the command dispatcher with basic integration tests.
"""

import pytest


def _await_channel_value(fprime_test_api, channel, timeout=10):
    """Await a fresh SystemResources channel update and return its value, failing clearly on timeout"""
    name = fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + channel
    result = fprime_test_api.await_telemetry(name, start="NOW", timeout=timeout)
    if result is None:
        pytest.fail(f"No telemetry received for {name} within {timeout}s")
    return result.get_val()


def test_send_systemResources_command(fprime_test_api):
    """Test that commands may be sent

    Tests command send, dispatch, and receipt using send_and_assert command with a pair of CmdDispatcher commands.

    SystemResources.Enable, <Disabled> (read SystemResources telemetry confirm value stale or stop)
    SystemResources.Enable, <Enabled>  (read SystemResources telemetry confirm value changing)

    """

    ## Verify memory usage Mem_total and Mem_used and Non_Volatile_total and Non_Volatile_free greater than a certain value 1KB
    memory_values = {
        channel: _await_channel_value(fprime_test_api, channel)
        for channel in [
            "MEMORY_TOTAL",
            "MEMORY_USED",
            "NON_VOLATILE_TOTAL",
            "NON_VOLATILE_FREE",
        ]
    }
    print("MEMORY VALUES (KB):", memory_values)
    for channel, value in memory_values.items():
        assert int(value) >= 1, f"{channel} reported {value} KB, expected >= 1 KB"

    # CPU percentages are informational only: per-core load is frequently below 1 percent
    cpu_values = {
        channel: _await_channel_value(fprime_test_api, channel)
        for channel in ["CPU", "CPU_00", "CPU_01", "CPU_02", "CPU_03"]
    }
    print("CPU VALUES (percent):", cpu_values)

    # Start command here:
    # Current channels before disable
    CPU_resources1 = fprime_test_api.await_telemetry(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "CPU", start="NOW"
    )
    CPU_percent1 = fprime_test_api.get_telemetry_pred(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "CPU",
        CPU_resources1,
    )
    print("CPU RESOURCES1: ", CPU_resources1)
    print("PERCENT: ", CPU_percent1)

    fprime_test_api.clear_histories()  # will clear all history (can read telemetry channel again with latest value.  otherwise still have old value)

    # Expect number still changing after clear_history
    fprime_test_api.await_telemetry(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "CPU", start="NOW"
    )

    ##### Command Disabled SystemResources.ENABLE command (DISABLED)
    fprime_test_api.send_and_assert_command(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "ENABLE",
        ["DISABLED"],
    )

    # Expect number no change (stale or stop) after Disable
    fprime_test_api.await_telemetry(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "CPU", start="NOW"
    )

    fprime_test_api.await_telemetry(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "CPU", start="NOW"
    )

    ##### Command Disabled SystemResources.ENABLE command (ENABLED)
    fprime_test_api.send_and_assert_command(
        fprime_test_api.get_mnemonic("Svc.SystemResources") + "." + "ENABLE",
        ["ENABLED"],
    )
