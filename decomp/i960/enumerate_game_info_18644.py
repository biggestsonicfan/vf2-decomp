#!/usr/bin/env python3
"""Enumerate ROM-backed fa_game_info 0x18644 fighter-state corridors.

Usage:
    python3 decomp/i960/enumerate_game_info_18644.py \
        ./build/vf2i960 /path/to/vf2-roms output.csv

The tool reconstructs the calibrated 0x164ac caller boundary, writes the real
fighter state fields at fighter+0x1a4, enumerates all subsets of bits 1, 2 and
4 on top of state bit 8, and runs pure ROM to the scheduler return for both
countdown values and both mode-bit-6 values.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import struct
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

REGION_SIZES_OFFSET = 8432
REGION_DATA_OFFSET = 8504
WORK_RAM_REGION = 2
WORK_RAM_BASE = 0x00500000
SNAPSHOT_VERSION = 6
STATE_OFFSET = 0x1A4
FIGHTER0_PTR_ADDRESS = 0x00500804
FIGHTER1_PTR_ADDRESS = 0x00500808
COUNTDOWN_ADDRESS = 0x0050A0B6
MODE_BASE_PTR_ADDRESS = 0x0050016C
MODE_OFFSET = 0x3351
SCHEDULER_RETURN = 0x00010DCC
GAME_INFO_CALL = 0x000164AC
META_INSTRUCTIONS = 44
META_CALLS = 52
META_RETURNS = 60
IP_OFFSET = 20


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("output_csv", type=Path)
    parser.add_argument("--workers", type=int, default=8)
    return parser.parse_args()


def run(command: list[str]) -> None:
    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stdout}\n{completed.stderr}"
        )


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def snapshot_layout(data: bytes) -> tuple[int, list[int]]:
    version = u32(data, 8)
    if version != SNAPSHOT_VERSION:
        raise RuntimeError(
            f"unsupported snapshot version {version}; expected {SNAPSHOT_VERSION}"
        )
    sizes = list(struct.unpack_from("<18I", data, REGION_SIZES_OFFSET))
    work = REGION_DATA_OFFSET + sum(sizes[:WORK_RAM_REGION])
    return work, sizes


def work_offset(data: bytes, address: int) -> int:
    base, sizes = snapshot_layout(data)
    size = sizes[WORK_RAM_REGION]
    if not WORK_RAM_BASE <= address < WORK_RAM_BASE + size:
        raise RuntimeError(f"0x{address:08x} is outside Work RAM")
    return base + address - WORK_RAM_BASE


def read_work_u32(data: bytes, address: int) -> int:
    return u32(data, work_offset(data, address))


def write_work_u32(data: bytearray, address: int, value: int) -> None:
    struct.pack_into("<I", data, work_offset(data, address), value)


def write_work_u8(data: bytearray, address: int, value: int) -> None:
    data[work_offset(data, address)] = value & 0xFF


def architectural_signature(snapshot: bytes) -> str:
    normalized = bytearray(snapshot)
    normalized[META_INSTRUCTIONS : META_RETURNS + 8] = b"\0" * (
        META_RETURNS + 8 - META_INSTRUCTIONS
    )
    return hashlib.sha256(normalized).hexdigest()[:16]


def build_boundary(binary: Path, roms: Path, directory: Path) -> Path:
    fifth = directory / "fifth.vf2snap"
    boundary = directory / "game_info_164ac.vf2snap"
    run([str(binary), "native-fifth-dispatch", str(roms), str(fifth)])
    run(
        [
            str(binary),
            "resume-trace",
            str(roms),
            str(fifth),
            "20000000",
            "4294967295",
            "0x80000000",
            "0",
            "0",
            str(boundary),
            hex(GAME_INFO_CALL),
        ]
    )
    if u32(boundary.read_bytes(), IP_OFFSET) != GAME_INFO_CALL:
        raise RuntimeError("failed to reconstruct 0x164ac boundary")
    return boundary


def enumerate_one(
    binary: Path,
    roms: Path,
    base: bytes,
    base_instructions: int,
    fighter0: int,
    fighter1: int,
    mode_address: int,
    state0: int,
    state1: int,
    countdown: int,
    mode_bit6: int,
    root: Path,
) -> dict[str, object]:
    data = bytearray(base)
    write_work_u32(data, fighter0 + STATE_OFFSET, state0)
    write_work_u32(data, fighter1 + STATE_OFFSET, state1)
    write_work_u8(data, COUNTDOWN_ADDRESS, countdown)
    original_mode = base[work_offset(base, mode_address)]
    mode = original_mode | 0x40 if mode_bit6 else original_mode & ~0x40
    write_work_u8(data, mode_address, mode)

    with tempfile.TemporaryDirectory(dir=root) as temporary:
        directory = Path(temporary)
        input_snapshot = directory / "input.vf2snap"
        output_snapshot = directory / "output.vf2snap"
        input_snapshot.write_bytes(data)
        run(
            [
                str(binary),
                "resume-trace",
                str(roms),
                str(input_snapshot),
                "1000000",
                "4294967295",
                "0",
                "0",
                "0",
                str(output_snapshot),
                hex(SCHEDULER_RETURN),
            ]
        )
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
    states = [0x100, 0x102, 0x104, 0x106, 0x110, 0x112, 0x114, 0x116]

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        boundary = build_boundary(args.binary, args.rom_directory, root).read_bytes()
        base_instructions = u64(boundary, META_INSTRUCTIONS)
        fighter0 = read_work_u32(boundary, FIGHTER0_PTR_ADDRESS)
        fighter1 = read_work_u32(boundary, FIGHTER1_PTR_ADDRESS)
        mode_address = read_work_u32(boundary, MODE_BASE_PTR_ADDRESS) + MODE_OFFSET
        jobs = [
            (state0, state1, countdown, mode_bit6)
            for state0 in states
            for state1 in states
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
                    fighter0,
                    fighter1,
                    mode_address,
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
        "state0",
        "state1",
        "countdown",
        "mode_bit6",
        "ip",
        "caller_to_task",
        "final_instructions",
        "calls",
        "returns",
        "signature",
    ]
    with args.output_csv.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {len(rows)} ROM-backed cases to {args.output_csv}")


if __name__ == "__main__":
    main()
