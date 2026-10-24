---
name: integration-testing
description: Author F´ integration test (InT) scripts using the GDS Python testing framework (fprime_gds.common.testing_fw). Covers pytest setup, the IntegrationTestAPI fixture, predicates, commands, events, telemetry, file operations, sequences, and best practices.
triggers: ["model", "user"]
---

# F´ Integration Testing Skill

Use this skill when writing or modifying integration test scripts for an F´ deployment. Integration tests exercise the full flight software through the GDS, sending commands and asserting on events and telemetry.

## Prerequisites

- The deployment must be built (`fprime-util build`).
- The GDS must be running against the deployment (`fprime-gds`).
- Tests are run via `pytest` with the deployment's dictionary available.

## File Layout

Place integration test files in your deployment or component directory:

```
<Deployment>/test/int/test_<name>.py
# or for component-level InT:
<Component>/test/int/test_<name>.py
```

No `conftest.py` is needed. The `fprime_gds.common.testing_fw.pytest_integration` plugin provides the `fprime_test_api` fixture automatically.

## Running Tests

```bash
# Start the GDS (in another terminal or background):
fprime-gds

# Run integration tests:
pytest <path>/test/int/test_<name>.py
```

The pytest plugin adds GDS connection options (e.g. `--dictionary`, `--ip-address`, `--ip-port`). These long-form flags can be passed directly to pytest.

## Writing a Test Function

Every integration test function receives the `fprime_test_api` fixture. This fixture:
1. Connects to the running GDS pipeline (session-scoped).
2. Clears test-specific histories at the start of each test (function-scoped).
3. Logs the test case name.

### Minimal Example

```python
def test_send_command(fprime_test_api):
    """Test that NO_OP command completes successfully."""
    fprime_test_api.send_and_assert_command("cmdDisp.CMD_NO_OP", max_delay=0.1)
    assert fprime_test_api.get_command_test_history().size() == 1
```

## Core API Methods

### Sending Commands

```python
# Send a command (fire-and-forget, no completion check):
fprime_test_api.send_command("component.COMMAND_NAME", ["arg1", "arg2"])

# Send and assert command dispatched + completed (recommended):
fprime_test_api.send_and_assert_command(
    "component.COMMAND_NAME",
    args=["arg1", "arg2"],
    max_delay=0.1,   # max seconds between dispatch and completion
    timeout=5,       # seconds to wait for events
)

# Send command and await/assert specific events:
fprime_test_api.send_and_assert_event(
    "component.COMMAND_NAME",
    args=["arg1"],
    events=["component.SomeEvent"],
    timeout=5,
)

# Send command and await/assert specific telemetry:
fprime_test_api.send_and_assert_telemetry(
    "component.COMMAND_NAME",
    args=[],
    channels="component.SomeChannel",
    timeout=5,
)
```

### Searching and Asserting on Events

```python
# Await a single event (returns EventData or None on timeout):
result = fprime_test_api.await_event("component.EventName", timeout=5)

# Await event with argument matching:
result = fprime_test_api.await_event("component.EventName", args=[expected_val, None])
# Use None for "don't care" arguments.

# Await a sequence of events in order:
results = fprime_test_api.await_event_sequence(
    ["component.Event1", "component.Event2"],
    timeout=10,
)

# Await a count of events:
results = fprime_test_api.await_event_count(3, "component.EventName", timeout=10)

# Assert (raises on failure) variants:
fprime_test_api.assert_event("component.EventName", timeout=5)
fprime_test_api.assert_event_sequence([...], timeout=10)
fprime_test_api.assert_event_count(3, "component.EventName", timeout=10)
```

### Searching and Asserting on Telemetry

```python
# Await a telemetry channel update:
result = fprime_test_api.await_telemetry("component.ChannelName", timeout=5)

# Await channel with specific value:
result = fprime_test_api.await_telemetry("component.ChannelName", value=42, timeout=5)

# Await a count of telemetry updates:
results = fprime_test_api.await_telemetry_count(5, "component.ChannelName", timeout=10)

# Await a sequence of telemetry updates:
results = fprime_test_api.await_telemetry_sequence(
    [fprime_test_api.get_telemetry_pred("component.Channel", i) for i in range(5)],
    timeout=10,
)

# Assert variants:
fprime_test_api.assert_telemetry("component.ChannelName", value=42, timeout=5)
fprime_test_api.assert_telemetry_count(5, "component.ChannelName", timeout=10)
fprime_test_api.assert_telemetry_sequence([...], timeout=10)
```

### File Operations

```python
# Upload a file to FSW and wait for completion:
fprime_test_api.uplink_file_and_await_completion(
    "local/path/file.bin", "/remote/dest/file.bin", timeout=100
)

# Upload without waiting:
fprime_test_api.uplink_file("local/path/file.bin", "/remote/dest/file.bin")

# Upload a sequence file (compiles .seq to .bin, then uplinks):
fprime_test_api.uplink_sequence_and_await_completion(
    "path/to/sequence.seq", "/remote/dest/sequence.bin", timeout=100
)
```

### Sequence Execution

```python
import subprocess

# Compile sequence:
assert subprocess.run([
    "fprime-seqgen",
    "--dictionary", str(fprime_test_api.dictionaries.dictionary_path),
    str(sequence_path),
    output_bin_path,
]).returncode == 0, "Failed to run fprime-seqgen"

# Upload and execute:
fprime_test_api.uplink_file_and_await_completion(output_bin_path, "/tmp/seq.bin", timeout=100)
fprime_test_api.send_and_assert_command(
    "cmdSeq.CS_RUN", args=["/tmp/seq.bin", "BLOCK"], max_delay=5
)
```

## Predicates

The `predicates` module provides composable filter/assertion objects. Import it:

```python
from fprime_gds.common.testing_fw import predicates
```

### Comparison Predicates

```python
predicates.less_than(value)
predicates.greater_than(value)
predicates.equal_to(value)
predicates.not_equal_to(value)
predicates.less_than_or_equal_to(value)
predicates.greater_than_or_equal_to(value)
predicates.within_range(lower, upper)
```

### Set Predicates

```python
predicates.is_a_member_of([val1, val2, val3])
predicates.is_not_a_member_of([val1, val2])
```

### Logic Predicates (Combinators)

```python
predicates.always_true()
predicates.invert(pred)               # NOT
predicates.satisfies_all([p1, p2])    # AND
predicates.satisfies_any([p1, p2])    # OR
```

### Data Object Predicates

```python
# Event predicate — filter/match events by id, args, severity, time:
predicates.event_predicate(id_pred=None, args_pred=None, severity_pred=None, time_pred=None)

# Telemetry predicate — filter/match channels by id, value, time:
predicates.telemetry_predicate(id_pred=None, value_pred=None, time_pred=None)

# Args predicate — match event argument lists:
predicates.args_predicate([expected_arg1, None, predicates.greater_than(0)])
# Use None for don't-care positions.
```

### Building Predicates via the API

The API provides convenience methods that auto-translate mnemonics/IDs:

```python
# Build an event predicate:
e_pred = fprime_test_api.get_event_pred(
    event="component.EventName",  # mnemonic, ID, or predicate
    args=[value, None],           # list of values/predicates/None
    severity=EventSeverity.COMMAND,
    time_pred=predicates.greater_than_or_equal_to(some_time),
)

# Build a telemetry predicate:
t_pred = fprime_test_api.get_telemetry_pred(
    channel="component.ChannelName",  # mnemonic, ID, or predicate
    value=predicates.within_range(0, 100),
    time_pred=None,
)
```

## Event Severity

```python
from fprime_gds.common.utils.event_severity import EventSeverity

EventSeverity.COMMAND       # command-related events
EventSeverity.ACTIVITY_LO   # low-priority activity
EventSeverity.ACTIVITY_HI   # high-priority activity
EventSeverity.WARNING_LO    # low-priority warning
EventSeverity.WARNING_HI    # high-priority warning
EventSeverity.DIAGNOSTIC    # diagnostic-level
EventSeverity.FATAL         # fatal events
```

## Search Scope: `start` and `timeout`

All searches accept `start` and `timeout` to control scope:

- **`start`**: Where to begin searching existing history.
  - `start=0`: search from the beginning of history.
  - `start="NOW"` or `start=IntegrationTestAPI.NOW`: skip existing history, only await future items.
  - `start=<TimeType>`: search items at or after this FSW timestamp.
  - `start=<predicate>`: search from the first item satisfying the predicate.
- **`timeout`**: How many seconds to await future items (0 = don't wait).

**Defaults**:
- `await_*` methods: `start="NOW"`, `timeout=5` — only search future items for 5s.
- `assert_*` methods: `start=None` (beginning), `timeout=0` — only search current history.

## Sub-Histories

Create filtered sub-histories for targeted searches:

```python
from fprime_gds.common.utils.event_severity import EventSeverity

# Create a sub-history that only captures COMMAND severity events:
cmd_filter = fprime_test_api.get_event_pred(severity=EventSeverity.COMMAND)
cmd_subhist = fprime_test_api.get_event_subhistory(cmd_filter)

# Use in searches:
fprime_test_api.assert_event_count(3, history=cmd_subhist)

# De-register when done:
fprime_test_api.remove_event_subhistory(cmd_subhist)

# Telemetry sub-histories work the same:
ch_filter = fprime_test_api.get_telemetry_pred("component.Channel")
ch_subhist = fprime_test_api.get_telemetry_subhistory(ch_filter)
fprime_test_api.assert_telemetry_count(5, history=ch_subhist)
fprime_test_api.remove_telemetry_subhistory(ch_subhist)
```

## Assert Helpers

```python
# Assert on a boolean condition (raises AssertionError on failure):
fprime_test_api.test_assert(condition, "description of check")

# Assert using a predicate on a value:
fprime_test_api.predicate_assert(predicates.less_than(10), value, "value should be < 10")

# Use expect=True to NOT raise (returns True/False, useful for accumulation):
all_passed = True
all_passed &= fprime_test_api.test_assert(x > 0, "x positive", expect=True)
all_passed &= fprime_test_api.test_assert(y > 0, "y positive", expect=True)
fprime_test_api.test_assert(all_passed, "All checks passed")
```

## TimeType Operations

Event and telemetry objects carry FSW timestamps (`TimeType`). They support arithmetic and comparison:

```python
from fprime_gds.common.models.serialize.time_type import TimeType

t0 = TimeType()       # 0.0 seconds
t1 = t0 + 1          # 1.0 seconds
t3 = t0 + 3          # 3.0 seconds
t2 = t3 - t1         # 2.0 seconds

# Comparisons:
t1 > 0               # True
t3 >= t2             # True

# Use with search results:
results = fprime_test_api.await_telemetry_sequence(["Channel"] * 5, timeout=10)
for i in range(1, len(results)):
    delay = results[i].get_time() - results[i-1].get_time()
    assert delay < 2, f"Gap too large: {delay}"
```

## Deployment Configuration

For deployments that remap component names, use a JSON config file:

```python
# Pass --deployment-config <path> to pytest, then in test code:
mnemonic = fprime_test_api.get_mnemonic("Svc.CommandDispatcher")
fprime_test_api.send_and_assert_command(
    mnemonic + "." + "CMD_NO_OP", max_delay=5
)
```

## History Management

```python
# Clear all test histories (between test phases):
fprime_test_api.clear_histories()

# Clear histories before a timestamp:
fprime_test_api.clear_histories(time_stamp=some_time)

# Access histories directly:
cmd_hist = fprime_test_api.get_command_test_history()
tlm_hist = fprime_test_api.get_telemetry_test_history()
evt_hist = fprime_test_api.get_event_test_history()
```

## Logging

```python
# Log a message to the test log:
fprime_test_api.log("Custom message for the test log")
fprime_test_api.log("Warning message", color="FF8800")
```

## Data Flow Verification

```python
# Wait for any telemetry to arrive (useful at test start):
fprime_test_api.wait_for_dataflow(count=1, timeout=120)
```

## Anti-Patterns to Avoid

1. **Do NOT assert zero-count with timeout**: `assert_telemetry_count(0, start="NOW", timeout=5)` exits immediately because 0 items are already found. Instead:
   ```python
   import time
   time.sleep(5)
   fprime_test_api.assert_telemetry_count(0)  # no timeout, searches existing only
   ```

2. **Do NOT specify timestamps in sequence searches**: Timestamps change between runs. Assert timing after the search:
   ```python
   results = fprime_test_api.await_telemetry_sequence(seq, timeout=10)
   for i in range(1, len(results)):
       assert results[i].get_time() - results[i-1].get_time() < max_gap
   ```

3. **Do NOT create no-scope searches**: Setting `start="NOW"` with `timeout=0` searches nothing.

4. **Do NOT use `invert()` to simulate complementary comparisons**: `invert(less_than(8))` is NOT the same as `greater_than_or_equal_to(8)` because non-comparable types (strings) return False from both.

## Complete Example: Component Integration Test

```python
"""test_my_component.py: Integration tests for MyComponent."""

import time
from fprime_gds.common.testing_fw import predicates
from fprime_gds.common.utils.event_severity import EventSeverity


def test_is_streaming(fprime_test_api):
    """Verify flight software is actively streaming telemetry."""
    results = fprime_test_api.assert_telemetry_count(5, timeout=10)
    for result in results:
        msg = f"received channel {result.get_id()} update: {result.get_str()}"
        fprime_test_api.log(msg)


def test_command_dispatch(fprime_test_api):
    """Test basic command dispatch and completion."""
    fprime_test_api.send_and_assert_command("cmdDisp.CMD_NO_OP", max_delay=0.1)
    assert fprime_test_api.get_command_test_history().size() == 1


def test_command_with_args(fprime_test_api):
    """Test command with string argument and verify event."""
    events = [
        fprime_test_api.get_event_pred("cmdDisp.NoOpStringReceived", ["Hello World"])
    ]
    fprime_test_api.send_and_assert_command(
        "cmdDisp.CMD_NO_OP_STRING",
        args=["Hello World"],
        max_delay=0.1,
        events=events,
    )


def test_telemetry_ascending(fprime_test_api):
    """Verify a counter channel produces ascending values."""
    count_pred = predicates.greater_than(9)
    results = fprime_test_api.await_telemetry_count(
        count_pred, "blockDrv.BD_Cycles", timeout=15
    )
    last = None
    for result in results:
        if last is not None:
            assert result.get_val() > last.get_val(), "Counter not ascending"
        last = result


def test_event_filtering(fprime_test_api):
    """Verify events of a specific severity are received."""
    cmd_events = fprime_test_api.get_event_pred(severity=EventSeverity.COMMAND)
    fprime_test_api.send_and_assert_command("cmdDisp.CMD_NO_OP")
    time.sleep(1.5)
    fprime_test_api.assert_event_count(
        predicates.greater_than(0), cmd_events
    )
```

## Checklist for Writing Integration Tests

1. Define the test function accepting `fprime_test_api` as its sole argument.
2. Use fully-qualified mnemonics (`component.COMMAND`) or `get_mnemonic()` for portability.
3. Always specify `timeout` for await/assert calls that wait for future data.
4. Use `max_delay` in `send_and_assert_command` to catch timing regressions.
5. Use predicates for flexible value matching rather than exact comparisons where appropriate.
6. Use `expect=True` pattern for multiple soft checks, then a final hard assert.
7. Clean up any created sub-histories with `remove_*_subhistory()`.
8. Do not use `time.sleep()` as a substitute for proper await calls — use it only when asserting absence of data.
9. Keep tests independent — each test function gets a fresh (cleared) history.
10. Use `fprime_test_api.log()` for diagnostic messages in the test log.
