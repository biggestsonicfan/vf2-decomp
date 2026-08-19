#!/usr/bin/env python3
"""Enumerate ROM-backed fa_game_info 0x18644 fighter-state corridors."""

from __future__ import annotations

import argparse
import csv
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import sys

TOOLS = Path(__file__).resolve().parent / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from game_info_18644_common import (  # noqa: E402
    ENUMERATED_STATES,
    FIGHTER0_PTR_ADDRESS,
    FIGHTER1_PTR_ADDRESS,
    META_CALLS,
    META_INSTRUCTIONS,
    META_RETURNS,
    MODE_BASE_PTR_ADDRESS,
    MODE_OFFSET,
    SCHEDULER_RETURN,
    architectural_signature,
    build_boundary,
    make_fixture,
    read_work_u32,
    resume_rom,
    u32,
    u64,
    IP_OFFSET,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("output_csv", type=Path)
    parser.add_argument("--workers", type=int, default=8)
    return parser.parse_args()


def enumerate_one(
    binary: Path,
    roms: Path,
    base: bytes,
    base_instructions: int,
    state0: int,
    state1: int,
    countdown: int,
    mode_bit6: int,
    root: Path,
) -> dict[str, object]:
    with tempfile.TemporaryDirectory(dir=root) as temporary:
        directory = Path(temporary)
        input_snapshot = directory / "input.vf2snap"
        output_snapshot = directory / "output.vf2snap"
        input_snapshot.write_bytes(make_fixture(base, state0, state1, countdown, mode_bit6))
        resume_rom(binary, roms, input_snapshot, output_snapshot, SCHEDULER_RETURN)
        result = output_snapshot.read_bytes()

    instructions = u64(result, META_INSTRUCTIONS)
    return {
        "state0": f"0x{state0:03x}",
        "state1": f"0x{state1:03x}",
        "countdown": countdown,
        "mode_bit6": mode_bit6,
        "ip": f"0x{u32(result, IP_OFFSET):08x}",
        "caller_to_task": instructions - base_instructions,
        "final_instructions": instructions,
        "calls": u64(result, META_CALLS),
        "returns": u64(result, META_RETURNS),
        "signature": architectural_signature(result),
    }


def main() -> None:
    args = parse_args()
    args.output_csv.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        boundary = build_boundary(args.binary, args.rom_directory, root).read_bytes()
        base_instructions = u64(boundary, META_INSTRUCTIONS)
        fighter0 = read_work_u32(boundary, FIGHTER0_PTR_ADDRESS)
        fighter1 = read_work_u32(boundary, FIGHTER1_PTR_ADDRESS)
        mode_address = read_work_u32(boundary, MODE_BASE_PTR_ADDRESS) + MODE_OFFSET
        print(
            f"boundary fighters=0x{fighter0:08x}/0x{fighter1:08x} "
            f"mode=0x{mode_address:08x}"
        )
        jobs = [
            (state0, state1, countdown, mode_bit6)
            for state0 in ENUMERATED_STATES
            for state1 in ENUMERATED_STATES
            for countdown in (0, 1)
            for mode_bit6 in (0, 1)
        ]
        rows: list[dict[str, object]] = []
        with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
            futures = [
                pool.submit(
                    enumerate_one,
                    args.binary,
                    args.rom_directory,
                    boundary,
                    base_instructions,
                    *job,
                    root,
                )
                for job in jobs
            ]
            for future in as_completed(futures):
                rows.append(future.result())

    rows.sort(
        key=lambda row: (
            int(str(row["state0"]), 16),
            int(str(row["state1"]), 16),
            int(row["countdown"]),
            int(row["mode_bit6"]),
        )
    )
    fields = [
        "state0", "state1", "countdown", "mode_bit6", "ip",
        "caller_to_task", "final_instructions", "calls", "returns", "signature",
    ]
    with args.output_csv.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {len(rows)} ROM-backed cases to {args.output_csv}")


if __name__ == "__main__":
    main()
