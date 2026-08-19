#!/usr/bin/env python3
"""Shared utilities for ROM/native differential work on fa_game_info 0x18644."""

from __future__ import annotations

import hashlib
import struct
import subprocess
from pathlib import Path

SNAPSHOT_VERSION = 6
REGION_SIZES_OFFSET = 8432
REGION_DATA_OFFSET = 8504
WORK_RAM_REGION = 2
WORK_RAM_BASE = 0x00500000

IP_OFFSET = 20
META_INSTRUCTIONS = 44
META_CALLS = 52
META_RETURNS = 60
META_INTERRUPT_ENTRIES = 68
META_INTERRUPT_RETURNS = 76

STATE_OFFSET = 0x1A4
FIGHTER0_PTR_ADDRESS = 0x00500804
FIGHTER1_PTR_ADDRESS = 0x00500808
COUNTDOWN_ADDRESS = 0x0050A0B6
MODE_BASE_PTR_ADDRESS = 0x0050016C
MODE_OFFSET = 0x3351
MODE_BIT6 = 0x40

GAME_INFO_FIRST_CALL = 0x000164AC
GAME_INFO_FIRST_RETURN = 0x000164B0
GAME_INFO_SECOND_CALL = 0x000164C0
GAME_INFO_SECOND_RETURN = 0x000164C4
GAME_INFO_CHILD = 0x00018644
SCHEDULER_RETURN = 0x00010DCC

ENUMERATED_STATES = (0x100, 0x102, 0x104, 0x106, 0x110, 0x112, 0x114, 0x116)


def run(command: list[str], *, quiet: bool = False) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    if not quiet and completed.stdout:
        print(completed.stdout, end="")
    return completed


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def write_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value)


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
    write_u32(data, work_offset(data, address), value)


def write_work_u8(data: bytearray, address: int, value: int) -> None:
    data[work_offset(data, address)] = value & 0xFF


def snapshot_counters(data: bytes) -> dict[str, int]:
    return {
        "instructions": u64(data, META_INSTRUCTIONS),
        "calls": u64(data, META_CALLS),
        "returns": u64(data, META_RETURNS),
        "interrupt_entries": u64(data, META_INTERRUPT_ENTRIES),
        "interrupt_returns": u64(data, META_INTERRUPT_RETURNS),
    }


def architectural_signature(data: bytes) -> str:
    normalized = bytearray(data)
    normalized[META_INSTRUCTIONS : META_INTERRUPT_RETURNS + 8] = b"\0" * (
        META_INTERRUPT_RETURNS + 8 - META_INSTRUCTIONS
    )
    return hashlib.sha256(normalized).hexdigest()[:16]


def make_fixture(base: bytes, state0: int, state1: int, countdown: int, mode_bit6: int) -> bytes:
    if countdown not in (0, 1) or mode_bit6 not in (0, 1):
        raise ValueError("countdown and mode_bit6 must be 0 or 1")
    data = bytearray(base)
    fighter0 = read_work_u32(base, FIGHTER0_PTR_ADDRESS)
    fighter1 = read_work_u32(base, FIGHTER1_PTR_ADDRESS)
    mode_address = read_work_u32(base, MODE_BASE_PTR_ADDRESS) + MODE_OFFSET
    write_work_u32(data, fighter0 + STATE_OFFSET, state0)
    write_work_u32(data, fighter1 + STATE_OFFSET, state1)
    write_work_u8(data, COUNTDOWN_ADDRESS, countdown)
    current_mode = data[work_offset(data, mode_address)]
    current_mode = current_mode | MODE_BIT6 if mode_bit6 else current_mode & ~MODE_BIT6
    write_work_u8(data, mode_address, current_mode)
    return bytes(data)


def build_boundary(binary: Path, roms: Path, directory: Path) -> Path:
    fifth = directory / "fifth.vf2snap"
    boundary = directory / "game_info_164ac.vf2snap"
    run([str(binary), "native-fifth-dispatch", str(roms), str(fifth)], quiet=True)
    run(
        [
            str(binary), "resume-trace", str(roms), str(fifth),
            "20000000", "4294967295", "0x80000000", "0", "0",
            str(boundary), hex(GAME_INFO_FIRST_CALL),
        ],
        quiet=True,
    )
    data = boundary.read_bytes()
    if u32(data, IP_OFFSET) != GAME_INFO_FIRST_CALL:
        raise RuntimeError("failed to reconstruct 0x164ac boundary")
    return boundary


def resume_rom(binary: Path, roms: Path, source: Path, output: Path, stop: int) -> None:
    run(
        [
            str(binary), "resume-trace", str(roms), str(source),
            "1000000", "4294967295", "0", "0", "0", str(output), hex(stop),
        ],
        quiet=True,
    )


def assert_snapshot_ip(data: bytes, expected: int, label: str) -> None:
    actual = u32(data, IP_OFFSET)
    if actual != expected:
        raise RuntimeError(f"{label}: expected IP 0x{expected:08x}, got 0x{actual:08x}")
