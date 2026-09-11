"""Integration tests of the segmented TC uplink (Segment Header + MAP reassembly), DESIGN §8.5 I1-I18.

Run against a deployment whose `fprime-gds` uses the `tc-segment` framing plugin (see conftest.py):

    -m segmented    I1-I12  Ref built with -DREF_TC_SEGMENTED=ON, or SubtopologyBuilds/Segmented
    -m feature_off  I13     default Ref (no Segment Header support)
    -m sdls         I14-I18 SubtopologyBuilds/Segmented built with -DSEGMENTED_SDLS=ON

Small `segment_size` values segment a genuine CMD_NO_OP_STRING packet (44 octets) so that a reassembled
packet is observable as a completed command; synthetic packets above FW_COM_BUFFER_MAX_SIZE (512) exercise
the nominal 1016/986-octet portions and are observable as `PacketsReassembled` plus the router's
`SerializationError` (the packet reached the router, which cannot fit it into a command buffer).
"""

import math
import time

import pytest

from tc_segment_plugin.control import Knobs

STRING_30 = "0123456789abcdefghijklmnopqrst"
NO_OP_STRING_PACKET_SIZE = (
    44  # 6 SP header + 2 descriptor + 4 opcode + 2 length + 30 characters
)
MAX_PACKET_SIZE = 4096  # TcMapCfg.MaxPacketSize
SPI_UNIT_VECTOR = 0x1234  # DESIGN §4.6 vector SPI, absent from the default SaMap (integration SPI is 1)


def no_op_string(dep, knobs, timeout=5.0):
    dep.control.arm(knobs)
    dep.api.send_and_assert_command(
        f"{dep.names.cmd_disp}.CMD_NO_OP_STRING",
        [STRING_30],
        timeout=timeout,
        commander=dep.names.cmd_disp,
    )
    dep.control.wait()


def send_no_op_string(dep, knobs):
    dep.control.arm(knobs)
    dep.api.send_command(f"{dep.names.cmd_disp}.CMD_NO_OP_STRING", [STRING_30])
    dep.control.wait()


def assert_large_packet_delivered(dep, before, count=1):
    """A reassembled packet above the command buffer size reaches the router and is refused there."""
    dep.await_counters(before, PacketsReassembled=count)
    dep.assert_event_count(dep.names.router, "SerializationError", count)


def segments_for(dep, size, portion=None):
    return math.ceil(size / (portion or dep.max_portion))


# ----------------------------------------------------------------------------------------------------
# I1-I12: segmented deployment, clear text
# ----------------------------------------------------------------------------------------------------


@pytest.mark.segmented
def test_i1_unsegmented_no_op(segmented):
    """I1: CMD_NO_OP in one UNSEGMENTED frame completes; PacketsReassembled +1."""
    dep = segmented
    before = dep.counters()
    dep.send_and_assert_no_op()
    dep.await_counters(before, PacketsReassembled=1)
    dep.assert_no_events_from(dep.names.reassembler, dep.names.deframer)


@pytest.mark.segmented
def test_i2_first_last(segmented):
    """I2: a command spanning FIRST/LAST completes; a 1500-octet packet is delivered as FIRST/LAST."""
    dep = segmented
    before = dep.counters()
    no_op_string(dep, Knobs(segment_size=24))  # 24 + 20
    dep.await_counters(before, PacketsReassembled=1)

    before = dep.counters()
    assert segments_for(dep, 1500) == 2
    dep.send_synthetic(1500)
    assert_large_packet_delivered(dep, before)
    dep.assert_no_events_from(dep.names.reassembler)


@pytest.mark.segmented
def test_i3_first_two_continuing_last(segmented):
    """I3: 3000-octet packet as FIRST + 2 CONTINUING + LAST is delivered."""
    dep = segmented
    before = dep.counters()
    no_op_string(dep, Knobs(segment_size=12))  # 12 + 12 + 12 + 8
    dep.await_counters(before, PacketsReassembled=1)

    before = dep.counters()
    dep.send_synthetic(3000, Knobs(segment_size=800))  # 800 + 800 + 800 + 600
    assert_large_packet_delivered(dep, before)
    dep.assert_no_events_from(dep.names.reassembler)


@pytest.mark.segmented
def test_i4_dropped_segment(segmented):
    """I4: a lost CONTINUING segment is detected at LAST (LengthMismatch), abandoned, next packet OK."""
    dep = segmented
    before = dep.counters()
    dep.send_synthetic(3000, Knobs(segment_size=800, drop=[1]))
    dep.assert_event(dep.names.reassembler, "LengthMismatch", [0, 2200, 3000])
    dep.await_counters(before, PacketsAbandoned=1)

    before = dep.counters()
    dep.send_and_assert_no_op()
    dep.await_counters(before, PacketsReassembled=1)


@pytest.mark.segmented
def test_i5_out_of_order(segmented):
    """I5: swapped CONTINUING segments reassemble to a packet with the right length but wrong bytes.

    The reassembler cannot detect this (no sequence numbers within a MAP, DESIGN §10): the packet is
    delivered and the command dispatcher rejects the garbled opcode. Nothing crashes and no command
    executes.
    """
    dep = segmented
    before = dep.counters()
    # 8-octet portions: [SP hdr + descriptor] [opcode + length + "01"] ["23456789"] ... ; swapping the
    # second and third segments puts "2345" where the opcode is expected.
    send_no_op_string(dep, Knobs(segment_size=8, swap=[1, 2]))
    dep.assert_event(dep.names.cmd_disp, "InvalidCommand", [0x32333435])
    dep.await_counters(before, PacketsReassembled=1)
    assert not dep.command_completed(timeout=1.0)
    dep.assert_no_events_from(dep.names.reassembler)

    dep.recover()


@pytest.mark.segmented
def test_i6_duplicate_first(segmented):
    """I6: a duplicated FIRST abandons the partial packet; the packet restarted by it completes."""
    dep = segmented
    before = dep.counters()
    no_op_string(dep, Knobs(segment_size=24, duplicate_first=True))
    dep.assert_event(dep.names.reassembler, "PacketAbandoned", [0, 24, "FIRST"])
    dep.await_counters(before, PacketsAbandoned=1, PacketsReassembled=1)


@pytest.mark.segmented
def test_i7_invalid_map(segmented):
    """I7: MAP 5 is not in the accepted table {0}: InvalidMapId(5), command not executed."""
    dep = segmented
    before = dep.counters()
    dep.send_no_op(Knobs(map_id=5))
    dep.assert_event(dep.names.reassembler, "InvalidMapId", [5])
    dep.await_counters(before, SegmentsDropped=1)
    assert not dep.command_completed(timeout=1.0)


@pytest.mark.segmented
def test_i8_oversize(segmented):
    """I8: a declared length of MaxPacketSize + 1 is refused before allocation (PacketTooLarge)."""
    dep = segmented
    before = dep.counters()
    size = MAX_PACKET_SIZE + 1
    count = segments_for(dep, size)
    dep.send_synthetic(size)
    dep.assert_event(
        dep.names.reassembler, "PacketTooLarge", [0, size, MAX_PACKET_SIZE]
    )
    # FIRST refused; every following segment finds the MAP idle.
    dep.assert_event_count(dep.names.reassembler, "UnexpectedSegment", count - 1)
    dep.await_counters(before, SegmentsDropped=count)
    dep.assert_no_events_from(dep.names.router)


@pytest.mark.segmented
def test_i9_no_first(segmented):
    """I9: CONTINUING and LAST without a FIRST are orphans: UnexpectedSegment x2."""
    dep = segmented
    before = dep.counters()
    send_no_op_string(
        dep, Knobs(segment_size=16, omit_first=True)
    )  # 16 + 16 + 12, FIRST withheld
    dep.assert_event(dep.names.reassembler, "UnexpectedSegment", [0, "CONTINUING"])
    dep.assert_event(dep.names.reassembler, "UnexpectedSegment", [0, "LAST"])
    dep.await_counters(before, SegmentsDropped=2)
    assert not dep.command_completed(timeout=1.0)


@pytest.mark.segmented
def test_i10_no_last_then_first(segmented):
    """I10: a packet whose LAST never arrives is abandoned by the next FIRST, which then completes."""
    dep = segmented
    before = dep.counters()
    send_no_op_string(dep, Knobs(segment_size=24, omit_last=True))
    no_op_string(dep, Knobs(segment_size=24))
    dep.assert_event(dep.names.reassembler, "PacketAbandoned", [0, 24, "FIRST"])
    dep.await_counters(before, PacketsAbandoned=1, PacketsReassembled=1)


@pytest.mark.segmented
def test_i11_pool_pressure(segmented):
    """I11: 2 x PoolBufferCount back-to-back packets; timing-independent invariants only."""
    dep = segmented
    pool_size = dep.pool_size()
    before = dep.counters()
    burst = 2 * pool_size
    for _ in range(burst):
        dep.api.send_command(f"{dep.names.cmd_disp}.CMD_NO_OP")
    dep.settle(1.0)
    dep.send_and_assert_no_op(
        timeout=10.0
    )  # (d) recovery after any drop, (e) deployment alive

    # (a) every frame accounted for exactly once
    total = burst + 1
    deadline = time.monotonic() + 15.0
    while True:
        after = dep.counters()
        delivered = after["PacketsReassembled"] - before["PacketsReassembled"]
        dropped = after["SegmentsDropped"] - before["SegmentsDropped"]
        if delivered + dropped == total or time.monotonic() > deadline:
            break
        time.sleep(0.2)
    assert (
        delivered + dropped == total
    ), f"{delivered} delivered + {dropped} dropped != {total}"
    assert after["PacketsAbandoned"] == before["PacketsAbandoned"]
    failures = dep.received_event_names().count(
        f"{dep.names.reassembler}.AllocationFailed"
    )
    assert dropped == failures, "every drop under pressure must be an AllocationFailed"

    # (b) and (c): pool never above its size, NoBuffs tracks AllocationFailed, nothing leaked. The pool
    # manager samples on its tick and downlinks changes only, so NoBuffs is checked against every
    # AllocationFailed of the session (the deployment received no uplink before the session started)
    # and CurrBuffs is awaited back at 0 in case a tick caught buffers in flight.
    dep.await_pool_counter(
        "NoBuffs",
        dep.all_event_names().count(f"{dep.names.reassembler}.AllocationFailed"),
    )
    assert dep.pool_counter("HiBuffs") <= pool_size
    dep.await_pool_counter("CurrBuffs", 0)


@pytest.mark.segmented
def test_i12_type_bc_rejected_upstream(segmented):
    """I12: a Type-BC frame never reaches TcDeframer (detector accepts Type-BD only); next packet OK."""
    dep = segmented
    before = dep.counters()
    dep.send_and_assert_no_op(Knobs(control_frame=True), timeout=10.0)
    dep.await_counters(before, PacketsReassembled=1)
    dep.assert_no_events_from(dep.names.deframer, dep.names.reassembler)


# ----------------------------------------------------------------------------------------------------
# I13: feature-off deployment
# ----------------------------------------------------------------------------------------------------


@pytest.mark.feature_off
def test_i13_feature_off_probe(feature_off):
    """I13: without the variant a Segment Header frame is forwarded as-is and refused as a Space Packet."""
    dep = feature_off
    dep.send_no_op(Knobs())  # Segment Header present, no reassembler in the deployment
    dep.assert_event(dep.names.space_packet_deframer, "InvalidPacket")
    assert not dep.command_completed(timeout=1.0)
    dep.assert_no_events_from(dep.names.reassembler)
    # The same command without a Segment Header is the baseline uplink and executes.
    dep.send_and_assert_no_op(Knobs(strip_sh=True))


# ----------------------------------------------------------------------------------------------------
# I14-I18: SDLS deployment (AES-256-GCM decryptor, SPI 1)
# ----------------------------------------------------------------------------------------------------


@pytest.mark.sdls
def test_i14_sdls_delivery(sdls):
    """I14: I1-I3 under AES-GCM: UNSEGMENTED, FIRST/LAST and FIRST+2xCONTINUING+LAST are delivered."""
    dep = sdls
    before = dep.counters()
    dep.send_and_assert_no_op()
    no_op_string(dep, Knobs(segment_size=24))
    no_op_string(dep, Knobs(segment_size=12))
    dep.await_counters(before, PacketsReassembled=3)

    before = dep.counters()
    assert segments_for(dep, 3000) == 4
    dep.send_synthetic(3000)
    assert_large_packet_delivered(dep, before)
    dep.assert_no_events_from(dep.names.reassembler, dep.names.sdls_deframer)


@pytest.mark.sdls
def test_i15_corrupt_mac_then_retransmit(sdls):
    """I15: a segment with a bad MAC is refused at the SDLS gate; its retransmission completes the packet."""
    dep = sdls
    before = dep.counters()
    no_op_string(
        dep, Knobs(segment_size=16, corrupt_mac=[1], retransmit=[1]), timeout=10.0
    )
    dep.assert_event_count(
        dep.names.sdls_deframer, "DecryptionFailed", 1, ["MAC_VERIFICATION_FAILURE"]
    )
    dep.await_counters(before, PacketsReassembled=1)
    dep.assert_no_events_from(dep.names.reassembler)


@pytest.mark.sdls
def test_i16_corrupt_mac_then_new_packet(sdls):
    """I16: the LAST segment fails authentication; the next FIRST abandons the partial packet."""
    dep = sdls
    before = dep.counters()
    send_no_op_string(dep, Knobs(segment_size=24, corrupt_mac=[1]))
    dep.assert_event(
        dep.names.sdls_deframer, "DecryptionFailed", ["MAC_VERIFICATION_FAILURE"]
    )
    no_op_string(dep, Knobs(segment_size=24))
    dep.assert_event(dep.names.reassembler, "PacketAbandoned", [0, 24, "FIRST"])
    dep.await_counters(before, PacketsAbandoned=1, PacketsReassembled=1)


@pytest.mark.sdls
def test_i17_tampered_segment_header(sdls):
    """I17: the Segment Header is part of the AAD: flags 01 -> 11 after protection fails authentication."""
    dep = sdls
    before = dep.counters()
    send_no_op_string(
        dep, Knobs(segment_size=24, tamper_sh={"index": 0, "value": 0xC0}, drop=[1])
    )
    dep.assert_event(
        dep.names.sdls_deframer, "DecryptionFailed", ["MAC_VERIFICATION_FAILURE"]
    )
    assert not dep.command_completed(timeout=1.0)
    dep.await_counters(before)
    dep.assert_no_events_from(dep.names.reassembler)

    dep.recover()


@pytest.mark.sdls
def test_i18_unknown_spi(sdls):
    """I18: SPI 0x1234 is not in the SaMap: DecryptionFailed(UNKNOWN_SA) once, nothing downstream."""
    dep = sdls
    before = dep.counters()
    dep.send_no_op(Knobs(spi=SPI_UNIT_VECTOR))
    dep.assert_event_count(
        dep.names.sdls_deframer, "DecryptionFailed", 1, ["UNKNOWN_SA"], timeout=5.0
    )
    assert not dep.command_completed(timeout=1.0)
    dep.settle(1.0)
    assert dep.events_from(dep.names.sdls_deframer) == [
        f"{dep.names.sdls_deframer}.DecryptionFailed"
    ]
    dep.await_counters(before)
    dep.assert_no_events_from(dep.names.reassembler)

    dep.recover()
