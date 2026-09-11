"""pytest fixtures for the segmented-TC integration tests (test_tc_segmented.py).

The deployment under test is driven through a `fprime-gds` started with the `tc-segment` framing plugin
(tc_segment_plugin/framing.py); the tests steer that plugin per packet through the JSON control file of
tc_segment_plugin/control.py. Instance names come from the deployment configuration passed with
`--deployment-config`, keyed by component type as the other F Prime reusable tests do:

    Svc.Ccsds.TcMapReassembler          MAP reassembler instance
    Svc.Ccsds.TcMapReassembler.pool     the Svc.BufferManager instance that owns the MAP packet pool
    Svc.Ccsds.TcDeframer.segmented      the TcDeframer instance running with tcSegmentHeaderPresent
    Svc.Ccsds.SpacePacketDeframer       Space Packet deframer instance
    Svc.FprimeRouter                    router instance
    Svc.Ccsds.CcsdsSdlsDeframer         SDLS deframer instance (SDLS deployments only)
    Svc.CommandDispatcher               command dispatcher instance
"""

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional

import pytest
from fprime_gds.common.testing_fw import predicates

from tc_segment_plugin import tc_frames as tf
from tc_segment_plugin.control import (
    Knobs,
    clear_knobs,
    control_file_path,
    wait_consumed,
    write_knobs,
)

# Dictionary discriminator of the SDLS deployment (Svc.Ccsds.AesGcmDecryptor has no events, channels or
# commands, so the selected decryptor can only be recognised by what is absent).
CLEAR_TEXT_DECRYPTOR_EVENT = "ComCcsdsSdls.decryptor.NullCipherInUse"
CLEAR_TEXT_ENCRYPTOR_EVENT = "ComCcsdsSdls.encryptor.NullCipherInUse"
KEY_MANAGER_FAILURE_EVENT = "Segmented.sdlsKeyManager.KeyReadFailed"
DECRYPTOR_EVENT_PREFIX = "ComCcsdsSdls.decryptor."
REPO_ROOT = Path(__file__).resolve().parents[5]
SDLS_KEY_FILE = (
    REPO_ROOT
    / "TestDeploymentsProject/SubtopologyBuilds/Segmented/test/int/sdls_test_key.bin"
)
SDLS_KEY_SIZE = 32
SDLS_KEY_FIRST_OCTET = 0x40

# Every channel used here is update-on-change: the reassembler counters are written when they move,
# the pool manager samples its counters on its rate-group tick (every fourth second in the segmented Ref
# deployment) and only a changed sample is downlinked. The timeouts are sized for that tick.
TLM_TIMEOUT = 15.0

COUNTERS = ("PacketsReassembled", "SegmentsDropped", "PacketsAbandoned")
# TcMapCfg.PoolBufferCount = MapChannelCount (1) + MaxPacketsInFlight (4); the pool manager reports it as
# TotalBuffs once, at its first tick, which precedes the test session (used when no sample was received).
POOL_BUFFER_COUNT = 5


def pytest_addoption(parser):
    parser.addoption(
        "--fsw-stdout-log",
        default=None,
        help="Flight software stdout capture (text events), used as a second source for events raised before"
        " the test session connected to the GDS (run-integration-tests writes gds-logs/fsw.stdout.log)",
    )


def pytest_configure(config):
    config.addinivalue_line(
        "markers",
        "segmented: scenarios I1-I12 and I19 against a segmented (SH mode) deployment",
    )
    config.addinivalue_line(
        "markers",
        "feature_off: scenario I13 against a default (no Segment Header) deployment",
    )
    config.addinivalue_line(
        "markers", "sdls: scenarios I14-I18 against the SEGMENTED_SDLS=ON deployment"
    )


@dataclass(frozen=True)
class Names:
    reassembler: str
    pool: str
    deframer: str
    space_packet_deframer: str
    router: str
    sdls_deframer: str
    cmd_disp: str


class PluginControl:
    """Publishes per-packet knobs to the tc-segment plugin running inside the GDS process."""

    def __init__(self):
        self.path = control_file_path()

    def reset(self):
        clear_knobs(self.path)

    def arm(self, knobs: Knobs):
        assert not self.path.exists(), f"stale plugin control file {self.path}"
        write_knobs(self.path, knobs)

    def wait(self, timeout: float = 10.0):
        assert wait_consumed(
            self.path, timeout
        ), f"plugin did not consume {self.path} within {timeout}s"


class Deployment:
    """Test-side view of the deployment: names, counters and plugin steering."""

    def __init__(self, api, names: Names, control: PluginControl, sdls: bool):
        self.api = api
        self.names = names
        self.control = control
        self.sdls = sdls

    # -- plugin ------------------------------------------------------------------------------------

    @property
    def max_portion(self) -> int:
        return tf.MAX_PORTION_SDLS if self.sdls else tf.MAX_PORTION_CLEAR

    def send_no_op(self, knobs: Optional[Knobs] = None):
        """Uplink CMD_NO_OP framed under `knobs`; returns once the plugin has framed it."""
        if knobs is not None:
            self.control.arm(knobs)
        self.api.send_command(f"{self.names.cmd_disp}.CMD_NO_OP")
        if knobs is not None:
            self.control.wait()

    def send_and_assert_no_op(
        self, knobs: Optional[Knobs] = None, timeout: float = 5.0
    ):
        if knobs is not None:
            self.control.arm(knobs)
        self.api.send_and_assert_command(
            f"{self.names.cmd_disp}.CMD_NO_OP",
            timeout=timeout,
            commander=self.names.cmd_disp,
        )
        if knobs is not None:
            self.control.wait()

    def recover(self):
        """Close a fault scenario: a plain CMD_NO_OP completes and its PacketsReassembled sample is consumed."""
        before = self.counters()
        self.send_and_assert_no_op()
        self.await_counters(before, PacketsReassembled=1)

    def send_synthetic(self, size: int, knobs: Optional[Knobs] = None, apid: int = 0):
        """Uplink a synthetic Space Packet of `size` octets (default APID FW_PACKET_COMMAND)."""
        knobs = knobs if knobs is not None else Knobs()
        knobs.payload = {"apid": apid, "size": size}
        self.send_no_op(knobs)

    # -- events --------------------------------------------------------------------------------------

    def event(self, instance: str, name: str) -> str:
        return f"{instance}.{name}"

    def assert_event(self, instance: str, name: str, args=None, timeout: float = 5.0):
        return self.api.assert_event(
            self.event(instance, name), args, start=0, timeout=timeout
        )

    def assert_event_count(
        self, instance: str, name: str, count: int, args=None, timeout: float = 5.0
    ):
        pred = self.api.get_event_pred(self.event(instance, name), args)
        return self.api.assert_event_count(count, pred, timeout=timeout)

    def received_event_names(self) -> List[str]:
        return [
            e.get_template().get_full_name()
            for e in self.api.get_event_test_history().retrieve()
        ]

    def events_from(self, instance: str) -> List[str]:
        prefix = f"{instance}."
        return [name for name in self.received_event_names() if name.startswith(prefix)]

    def assert_no_events_from(self, *instances: str):
        for instance in instances:
            found = self.events_from(instance)
            assert found == [], f"unexpected events from {instance}: {found}"

    def command_completed(self, timeout: float = 2.0) -> bool:
        pred = self.api.get_event_pred(f"{self.names.cmd_disp}.OpCodeCompleted")
        return self.api.await_event(pred, start=0, timeout=timeout) is not None

    # -- telemetry -----------------------------------------------------------------------------------

    def channel(self, instance: str, name: str) -> str:
        return f"{instance}.{name}"

    def latest(self, instance: str, name: str, default: Optional[int] = None) -> int:
        """Latest received value of a channel over the whole GDS session (aggregate history)."""
        full = self.channel(instance, name)
        value = default
        for item in self.api.aggregate_telemetry_history.retrieve():
            if item.get_template().get_full_name() == full:
                value = item.get_val()
        assert value is not None, f"no sample of {full} received yet"
        return value

    def all_event_names(self) -> List[str]:
        """Every event received over the whole GDS session (aggregate history)."""
        return [
            e.get_template().get_full_name()
            for e in self.api.aggregate_event_history.retrieve()
        ]

    def counters(self) -> Dict[str, int]:
        return {name: self.latest(self.names.reassembler, name) for name in COUNTERS}

    def await_value(
        self, instance: str, name: str, value: int, timeout: float = TLM_TIMEOUT
    ):
        pred = self.api.get_telemetry_pred(
            self.channel(instance, name), predicates.equal_to(value)
        )
        result = self.api.await_telemetry(pred, start=0, timeout=timeout)
        assert (
            result is not None
        ), f"{instance}.{name} did not reach {value} within {timeout}s (latest {self.latest(instance, name, 'n/a')})"
        return result

    def await_counters(self, before: Dict[str, int], **deltas: int):
        """Await every reassembler counter at before + delta (0 for the ones not named)."""
        for name in COUNTERS:
            expected = before[name] + deltas.get(name, 0)
            if deltas.get(name, 0):
                self.await_value(self.names.reassembler, name, expected)
        # Unchanged counters are update-on-change: nothing newer than `before` may have arrived.
        for name in COUNTERS:
            if not deltas.get(name, 0):
                assert (
                    self.latest(self.names.reassembler, name) == before[name]
                ), f"{name} changed unexpectedly"

    def settle(self, seconds: float = 1.0):
        time.sleep(seconds)

    def pool_size(self) -> int:
        """Pool capacity: TotalBuffs when a sample was received this session, else TcMapCfg.PoolBufferCount."""
        return self.latest(self.names.pool, "TotalBuffs", POOL_BUFFER_COUNT)

    def pool_counter(self, name: str) -> int:
        """Pool manager counter (HiBuffs, CurrBuffs, NoBuffs, EmptyBuffs); 0 until it first moves."""
        return self.latest(self.names.pool, name, 0)

    def await_pool_counter(self, name: str, value: int, timeout: float = TLM_TIMEOUT):
        """Wait for the pool manager to sample `name` at `value` on one of its ticks (or already be there)."""
        deadline = time.monotonic() + timeout
        while self.pool_counter(name) != value and time.monotonic() < deadline:
            time.sleep(0.2)
        assert (
            self.pool_counter(name) == value
        ), f"{self.names.pool}.{name} is {self.pool_counter(name)}, expected {value}"


@pytest.fixture(scope="session")
def tc_names(fprime_test_api_session) -> Names:
    api = fprime_test_api_session
    return Names(
        reassembler=api.get_mnemonic("Svc.Ccsds.TcMapReassembler"),
        pool=api.get_mnemonic("Svc.Ccsds.TcMapReassembler.pool"),
        deframer=api.get_mnemonic("Svc.Ccsds.TcDeframer.segmented"),
        space_packet_deframer=api.get_mnemonic("Svc.Ccsds.SpacePacketDeframer"),
        router=api.get_mnemonic("Svc.FprimeRouter"),
        sdls_deframer=api.get_mnemonic("Svc.Ccsds.CcsdsSdlsDeframer"),
        cmd_disp=api.get_mnemonic("Svc.CommandDispatcher"),
    )


@pytest.fixture(scope="session")
def plugin_control() -> PluginControl:
    control = PluginControl()
    control.reset()
    yield control
    control.reset()


@pytest.fixture(scope="session")
def event_names(fprime_test_api_session) -> List[str]:
    return list(fprime_test_api_session.pipeline.dictionaries.event_name.keys())


@pytest.fixture(scope="session")
def channel_names(fprime_test_api_session) -> List[str]:
    return list(fprime_test_api_session.pipeline.dictionaries.channel_name.keys())


@pytest.fixture(scope="session")
def sdls_deployment(fprime_test_api_session, event_names) -> bool:
    """True when the dictionary is the SEGMENTED_SDLS=ON one (AES-GCM decryptor, key manager)."""
    return (
        CLEAR_TEXT_DECRYPTOR_EVENT not in event_names
        and KEY_MANAGER_FAILURE_EVENT in event_names
    )


@pytest.fixture(scope="session")
def primed(fprime_test_api_session, tc_names, plugin_control, channel_names):
    """Make every reassembler counter observable once (they are update-on-change) before the tests.

    One rejected frame (MAP 5), one abandoned packet (FIRST without LAST superseded by the next
    UNSEGMENTED frame) and one completed packet touch SegmentsDropped, PacketsAbandoned and
    PacketsReassembled respectively, so their current values are known for the rest of the session.
    """
    api = fprime_test_api_session
    reassembler = tc_names.reassembler
    if f"{reassembler}.PacketsReassembled" not in channel_names:
        pytest.skip(f"{reassembler} is not part of this deployment")
    dep = Deployment(api, tc_names, plugin_control, sdls=False)
    start = api.get_telemetry_test_history().size()
    dep.send_no_op(Knobs(map_id=5))
    dep.send_synthetic(1500, Knobs(omit_last=True))
    api.send_and_assert_command(
        f"{tc_names.cmd_disp}.CMD_NO_OP", timeout=10, commander=tc_names.cmd_disp
    )
    for name in COUNTERS:
        found = api.await_telemetry(
            dep.channel(reassembler, name), start=start, timeout=TLM_TIMEOUT
        )
        assert found is not None, f"{reassembler}.{name} was not reported after priming"
    return True


@pytest.fixture
def segmented(
    primed, sdls_deployment, fprime_test_api, tc_names, plugin_control
) -> Deployment:
    """A segmented deployment (Ref REF_TC_SEGMENTED=ON or SubtopologyBuilds/Segmented) with primed counters."""
    return Deployment(fprime_test_api, tc_names, plugin_control, sdls=sdls_deployment)


@pytest.fixture
def feature_off(fprime_test_api, tc_names, plugin_control, channel_names) -> Deployment:
    """A deployment built without the segmented variant: the reassembler must be absent."""
    reassembler = tc_names.reassembler
    present = [name for name in channel_names if name.startswith(f"{reassembler}.")]
    assert (
        present == []
    ), f"{reassembler} is part of this deployment; not a feature-off build"
    return Deployment(fprime_test_api, tc_names, plugin_control, sdls=False)


def _text_events(path: Optional[str], full_name: str) -> int:
    """Occurrences of event `full_name` in a flight-software stdout capture (`(<instance>) <event> :` lines)."""
    if path is None or not Path(path).is_file():
        return 0
    instance, _, event = full_name.rpartition(".")
    marker = f"({instance}) {event} :"
    return sum(
        1
        for line in Path(path).read_text(errors="replace").splitlines()
        if marker in line
    )


@pytest.fixture
def sdls(request, segmented, sdls_deployment, event_names) -> Deployment:
    """SDLS deployment checks of DESIGN §8.5: run only against the AES-GCM build, verify after each scenario."""
    if CLEAR_TEXT_DECRYPTOR_EVENT in event_names:
        pytest.skip("clear-text decryptor selected")
    assert (
        sdls_deployment
    ), f"{KEY_MANAGER_FAILURE_EVENT} is not defined: not the SEGMENTED_SDLS=ON dictionary"
    key = SDLS_KEY_FILE.read_bytes()
    assert key == bytes(
        SDLS_KEY_FIRST_OCTET + i for i in range(SDLS_KEY_SIZE)
    ), f"unexpected key file {SDLS_KEY_FILE}"
    dep = segmented
    assert dep.sdls
    yield dep
    # After every scenario, over the whole session's raw event stream: no decryptor event at all (an
    # AES-GCM decryptor is silent; the name is undefined in this dictionary so it is matched by prefix),
    # at least one NullCipherInUse from the deliberately clear-text downlink encryptor, no key-manager
    # failure. NullCipherInUse is throttled at 5 and the encryptor handles its first five TM frames while
    # the deployment boots, before the test session is attached to the GDS: the flight-software stdout
    # capture (--fsw-stdout-log) is the second place those five reports can be found.
    received = dep.all_event_names()
    decryptor_events = [
        name for name in received if name.startswith(DECRYPTOR_EVENT_PREFIX)
    ]
    assert decryptor_events == [], f"decryptor events received: {decryptor_events}"
    fsw_log = request.config.getoption("--fsw-stdout-log")
    encryptor_reports = received.count(CLEAR_TEXT_ENCRYPTOR_EVENT) + _text_events(
        fsw_log, CLEAR_TEXT_ENCRYPTOR_EVENT
    )
    assert (
        encryptor_reports >= 1
    ), f"downlink encryptor did not report {CLEAR_TEXT_ENCRYPTOR_EVENT} (GDS stream or --fsw-stdout-log={fsw_log})"
    assert (
        received.count(KEY_MANAGER_FAILURE_EVENT) == 0
    ), "sdlsKeyManager reported KeyReadFailed"
    assert (
        _text_events(fsw_log, KEY_MANAGER_FAILURE_EVENT) == 0
    ), "sdlsKeyManager reported KeyReadFailed at boot"
