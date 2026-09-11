#!/usr/bin/env python3
"""Feature-off dictionary regression check for the Ref deployment.

Compares a candidate ``RefTopologyDictionary.json`` (Ref built with the default
``ComCcsds.Subtopology``) against a baseline dictionary and enforces the
feature-off rule of the TC Segment Header / MAP design: every dictionary
section is identical, except that ``events`` may gain the two ``TcDeframer``
Segment Header events, appended after every existing ``ComCcsds.tcDeframer``
event so that no pre-existing event ID moves.

Usage:
    check_dictionary_regression.py --baseline <json> --candidate <json>

Exit status 0 when the rule holds, 1 (with a report on stderr) otherwise.
"""

import argparse
import json
import sys

# Sections compared for strict equality (metadata carries build versions and is skipped)
STRICT_SECTIONS = (
    "typeDefinitions",
    "constants",
    "commands",
    "parameters",
    "telemetryChannels",
    "records",
    "containers",
    "telemetryPacketSets",
)

# Section that may differ, and the only additions it may carry
ADDITIVE_SECTION = "events"
ALLOWED_ADDED_EVENTS = frozenset(
    {
        "ComCcsds.tcDeframer.MissingSegmentHeader",
        "ComCcsds.tcDeframer.ControlFrameDropped",
    }
)
ADDITIVE_INSTANCE_PREFIX = "ComCcsds.tcDeframer."


def canonical(value):
    """Return a hashable canonical form of a JSON value."""
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def entry_key(entry):
    """Identify a dictionary entry by its name-like field."""
    for field in ("name", "qualifiedName"):
        if field in entry:
            return entry[field]
    return canonical(entry)


def index(section):
    """Map entry key -> canonical entry for a list section."""
    return {entry_key(entry): canonical(entry) for entry in section}


def compare_strict(name, baseline, candidate, errors):
    base = index(baseline.get(name, []))
    cand = index(candidate.get(name, []))
    for key in sorted(set(base) - set(cand)):
        errors.append(f"{name}: removed {key}")
    for key in sorted(set(cand) - set(base)):
        errors.append(f"{name}: added {key}")
    for key in sorted(set(base) & set(cand)):
        if base[key] != cand[key]:
            errors.append(f"{name}: changed {key}")


def compare_events(baseline, candidate, errors):
    base_events = baseline.get(ADDITIVE_SECTION, [])
    cand_events = candidate.get(ADDITIVE_SECTION, [])
    base = index(base_events)
    cand = index(cand_events)
    for key in sorted(set(base) - set(cand)):
        errors.append(f"events: removed {key}")
    for key in sorted(set(base) & set(cand)):
        if base[key] != cand[key]:
            errors.append(f"events: changed {key}")
    added = set(cand) - set(base)
    for key in sorted(added - ALLOWED_ADDED_EVENTS):
        errors.append(
            f"events: added {key} (only {sorted(ALLOWED_ADDED_EVENTS)} may be added)"
        )
    if added & ALLOWED_ADDED_EVENTS:
        base_max = max(
            (
                event["id"]
                for event in base_events
                if event["name"].startswith(ADDITIVE_INSTANCE_PREFIX)
            ),
            default=-1,
        )
        for event in cand_events:
            if event["name"] in added and event["id"] <= base_max:
                errors.append(
                    f"events: {event['name']} id {event['id']:#x} is not above every baseline "
                    f"{ADDITIVE_INSTANCE_PREFIX}* event id ({base_max:#x})"
                )
    return sorted(added)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--baseline", required=True, help="baseline RefTopologyDictionary.json"
    )
    parser.add_argument(
        "--candidate", required=True, help="candidate RefTopologyDictionary.json"
    )
    args = parser.parse_args(argv)

    with open(args.baseline, encoding="utf-8") as handle:
        baseline = json.load(handle)
    with open(args.candidate, encoding="utf-8") as handle:
        candidate = json.load(handle)

    errors = []
    for name in STRICT_SECTIONS:
        compare_strict(name, baseline, candidate, errors)
    added = compare_events(baseline, candidate, errors)

    if errors:
        print("Dictionary regression check FAILED:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1
    if added:
        print(
            f"Dictionary regression check passed: additive only, events added {added}"
        )
    else:
        print(
            "Dictionary regression check passed: dictionaries identical (metadata ignored)"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
