#!/usr/bin/env python3
"""Compare serialized snapshot meta counters that compare-snapshots does not enforce."""

from __future__ import annotations

import argparse
from pathlib import Path

from game_info_18644_common import architectural_signature, snapshot_counters


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("expected", type=Path)
    parser.add_argument("actual", type=Path)
    parser.add_argument("--signature", action="store_true", help="also compare normalized architectural signatures")
    args = parser.parse_args()

    expected = args.expected.read_bytes()
    actual = args.actual.read_bytes()
    left = snapshot_counters(expected)
    right = snapshot_counters(actual)
    equal = True
    for name in left:
        delta = right[name] - left[name]
        state = "MATCH" if delta == 0 else "DIFF"
        print(f"{name:18s} {state:5s} expected={left[name]} actual={right[name]} delta={delta:+d}")
        equal &= delta == 0
    if args.signature:
        a = architectural_signature(expected)
        b = architectural_signature(actual)
        print(f"architecture       {'MATCH' if a == b else 'DIFF ':5s} expected={a} actual={b}")
        equal &= a == b
    raise SystemExit(0 if equal else 1)


if __name__ == "__main__":
    main()
