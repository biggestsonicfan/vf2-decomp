#!/usr/bin/env python3
"""Diagnose first/second native-child accounting for one 0x18644 state pair."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

from game_info_18644_common import (
    GAME_INFO_FIRST_RETURN,
    GAME_INFO_SECOND_CALL,
    GAME_INFO_SECOND_RETURN,
    SCHEDULER_RETURN,
    architectural_signature,
    build_boundary,
    make_fixture,
    resume_rom,
    snapshot_counters,
)

HERE = Path(__file__).resolve().parent
CHILD_RUNNER = HERE / "run_game_info_child.py"


def run_child(
    binary: Path,
    roms: Path,
    source: Path,
    output: Path,
    return_address: int,
) -> None:
    completed = subprocess.run(
        [
            sys.executable,
            str(CHILD_RUNNER),
            str(binary),
            str(roms),
            str(source),
            str(output),
            "--return-address",
            hex(return_address),
        ],
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr or completed.stdout)


def compare_delta(expected: Path, actual: Path) -> tuple[bool, dict[str, int]]:
    expected_data = expected.read_bytes()
    actual_data = actual.read_bytes()
    expected_counters = snapshot_counters(expected_data)
    actual_counters = snapshot_counters(actual_data)
    architecture_equal = (
        architectural_signature(expected_data)
        == architectural_signature(actual_data)
    )
    deltas = {
        name: actual_counters[name] - expected_counters[name]
        for name in expected_counters
    }
    return architecture_equal, deltas


def diagnose_case(
    binary: Path,
    roms: Path,
    base: bytes,
    state0: int,
    state1: int,
    countdown: int,
    mode_bit6: int,
    directory: Path,
) -> None:
    stem = f"s{state0:03x}_{state1:03x}_cd{countdown}_m{mode_bit6}"
    start = directory / f"{stem}_start.vf2snap"
    reference = directory / f"{stem}_reference.vf2snap"
    child1 = directory / f"{stem}_child1.vf2snap"
    first_tail = directory / f"{stem}_first_tail.vf2snap"
    second_call = directory / f"{stem}_second_call.vf2snap"
    child2 = directory / f"{stem}_child2.vf2snap"
    second_tail = directory / f"{stem}_second_tail.vf2snap"

    start.write_bytes(make_fixture(base, state0, state1, countdown, mode_bit6))
    resume_rom(binary, roms, start, reference, SCHEDULER_RETURN)

    run_child(binary, roms, start, child1, GAME_INFO_FIRST_RETURN)
    resume_rom(binary, roms, child1, first_tail, SCHEDULER_RETURN)

    resume_rom(binary, roms, start, second_call, GAME_INFO_SECOND_CALL)
    run_child(binary, roms, second_call, child2, GAME_INFO_SECOND_RETURN)
    resume_rom(binary, roms, child2, second_tail, SCHEDULER_RETURN)

    first_architecture, first_delta = compare_delta(reference, first_tail)
    second_architecture, second_delta = compare_delta(reference, second_tail)
    print(
        f"0x{state0:03x}->0x{state1:03x} "
        f"cd={countdown} mode6={mode_bit6} "
        f"first_arch={'MATCH' if first_architecture else 'DIFF'} "
        f"first_delta={first_delta} "
        f"second_arch={'MATCH' if second_architecture else 'DIFF'} "
        f"second_delta={second_delta}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("state0", type=lambda value: int(value, 0))
    parser.add_argument("state1", type=lambda value: int(value, 0))
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--base", type=Path, help="reuse a calibrated 0x164ac snapshot")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        base = (
            args.base.read_bytes()
            if args.base is not None
            else build_boundary(args.binary, args.rom_directory, root).read_bytes()
        )
        pairs = [(args.state0, args.state1)]
        if args.reverse and args.state0 != args.state1:
            pairs.append((args.state1, args.state0))

        for state0, state1 in pairs:
            for countdown in (0, 1):
                for mode_bit6 in (0, 1):
                    diagnose_case(
                        args.binary,
                        args.rom_directory,
                        base,
                        state0,
                        state1,
                        countdown,
                        mode_bit6,
                        root,
                    )


if __name__ == "__main__":
    main()
