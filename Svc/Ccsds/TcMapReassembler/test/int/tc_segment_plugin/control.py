"""Per-packet fault-injection controls shared by the tc-segment plugin and the integration tests.

The GDS `comm` process (where the plugin frames uplink data) and the pytest process are separate, so
the tests steer the plugin through a small JSON control file under the working tree. The tests write
it atomically (temp file + rename) before sending a command; the plugin reads it when the next Space
Packet is framed and, unless `once` is false, deletes it. `wait_consumed` lets a test block until the
plugin has applied the knobs, which also orders "command framed" before "assert on flight events".

Knobs (all optional; segment indices refer to the nominal FIRST..LAST list, 0 = FIRST):

    segment_size       maximum Space Packet portion per frame (capped at the mode maximum)
    map_id             MAP ID override for every Segment Header of this packet
    spi                SDLS Security Parameter Index override for this packet
    payload            {"apid": int, "size": int}: replace the uplinked data by a synthetic packet
                       whose Space Packet totals `size` octets on the given APID
    drop               list of segment indices not transmitted
    swap               [i, j]: transmit segment j in place of i and vice versa
    duplicate_first    transmit FIRST twice
    omit_first         start with the second segment (CONTINUING/LAST without FIRST)
    omit_last          do not transmit the LAST segment
    corrupt_mac        list of segment indices whose MAC last octet is flipped (FECF recomputed)
    retransmit         list of segment indices transmitted a second time, uncorrupted, right after
    tamper_sh          {"index": i, "value": v}: overwrite the SH octet after SDLS protection
    strip_sh           omit the Segment Header octet from every frame
    control_frame      transmit a Type-BC Unlock frame before the packet
    empty_segment      transmit an UNSEGMENTED frame with an empty portion before the packet
    once               delete the control file after this packet (default true)
"""

import json
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

ENV_CONTROL_FILE = "TC_SEGMENT_PLUGIN_CONTROL_FILE"
ENV_SDLS_KEY_FILE = "TC_SEGMENT_PLUGIN_SDLS_KEY_FILE"
ENV_SDLS_SPI = "TC_SEGMENT_PLUGIN_SPI"

DEFAULT_CONTROL_FILE = Path(__file__).resolve().parent.parent / "tc_segment_control.json"


def control_file_path(explicit: Optional[str] = None) -> Path:
    if explicit:
        return Path(explicit)
    return Path(os.environ.get(ENV_CONTROL_FILE, str(DEFAULT_CONTROL_FILE)))


@dataclass
class Knobs:
    segment_size: Optional[int] = None
    map_id: Optional[int] = None
    spi: Optional[int] = None
    payload: Optional[Dict[str, int]] = None
    drop: List[int] = field(default_factory=list)
    swap: Optional[List[int]] = None
    duplicate_first: bool = False
    omit_first: bool = False
    omit_last: bool = False
    corrupt_mac: List[int] = field(default_factory=list)
    retransmit: List[int] = field(default_factory=list)
    tamper_sh: Optional[Dict[str, int]] = None
    strip_sh: bool = False
    control_frame: bool = False
    empty_segment: bool = False
    once: bool = True

    @classmethod
    def from_dict(cls, values: dict) -> "Knobs":
        unknown = set(values) - set(cls.__dataclass_fields__)
        if unknown:
            raise ValueError(f"unknown tc-segment control knobs: {sorted(unknown)}")
        return cls(**values)

    def to_dict(self) -> dict:
        defaults = Knobs()
        return {key: value for key, value in self.__dict__.items() if value != getattr(defaults, key)}


def write_knobs(path: Path, knobs: Knobs) -> None:
    """Atomically publish `knobs` at `path`."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    temp.write_text(json.dumps(knobs.to_dict(), indent=2) + "\n")
    os.replace(temp, path)


def read_knobs(path: Path) -> Knobs:
    """Read the knobs at `path`; absent or unreadable file means defaults. Deletes the file when `once`."""
    try:
        text = path.read_text()
    except OSError:
        return Knobs()
    try:
        knobs = Knobs.from_dict(json.loads(text))
    except (ValueError, TypeError) as error:
        raise RuntimeError(f"tc-segment control file {path} is invalid: {error}") from error
    if knobs.once:
        clear_knobs(path)
    return knobs


def clear_knobs(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def wait_consumed(path: Path, timeout: float = 10.0) -> bool:
    """Block until the plugin has consumed (deleted) the control file; False on timeout."""
    deadline = time.monotonic() + timeout
    while path.exists():
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.05)
    return True
