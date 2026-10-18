#!/usr/bin/env python3
"""Recover the VF2.1 18x11 sound-command selection matrix from local ROMs.

Uses MAME LOAD32_WORD layout and ROM pointers; never commits or writes
ROM bytes. A table entry is a candidate command, NOT a proven gameplay event.
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path
from zipfile import ZipFile

BUS_BASE = 0x02000000
REGION_BYTES = 0x400000
ROM_BYTES = 0x200000
PTR_TABLE = 0x0201E44C
PROFILE_COUNT = 18
COMMANDS_PER_PROFILE = 11
COMMAND_BYTES = COMMANDS_PER_PROFILE * 4
ROM_PAIR = ("mpr-17560.10", "mpr-17561.11")
VALID_PREFIXES = {0xAD23, 0xAD24}


class InvalidSoundMatrix(ValueError):
    """Unsupported, incomplete or malformed ROM sound-selection matrix."""


def reconstruct_main_data_primary(source: Path) -> bytes:
    """Load only the 4MB first main_data pair from vf2.zip or roms/vf2."""
    if source.is_dir():
        pieces = [(source / name).read_bytes() for name in ROM_PAIR]
    else:
        with ZipFile(source) as archive:
            pieces = [archive.read(name) for name in ROM_PAIR]
    if any(len(piece) != ROM_BYTES for piece in pieces):
        raise InvalidSoundMatrix("expected two 0x200000-byte primary main-data ROMs")
    lo, hi = pieces
    region = bytearray(REGION_BYTES)
    region[0::4] = lo[0::2]
    region[1::4] = lo[1::2]
    region[2::4] = hi[0::2]
    region[3::4] = hi[1::2]
    return bytes(region)


def u32(region: bytes, bus_address: int) -> int:
    offset = bus_address - BUS_BASE
    if offset < 0 or offset % 4 or offset + 4 > len(region):
        raise InvalidSoundMatrix(f"invalid ROM pointer/address 0x{bus_address:08x}")
    return struct.unpack_from("<I", region, offset)[0]


def decode_matrix(region: bytes) -> list[list[int]]:
    """Read null pointer index zero, followed by 18 profiles x 11 slots.

    Reject out-of-window pointers and foreign command families; a zero
    CELL is a valid empty selection. Static table availability is not
    evidence of an SFX naturally emitted by gameplay.
    """
    if len(region) < REGION_BYTES:
        raise InvalidSoundMatrix("primary main_data truncated")
    if u32(region, PTR_TABLE) != 0:
        raise InvalidSoundMatrix("expected null profile at pointer index 0")
    rows: list[list[int]] = []
    for index in range(1, PROFILE_COUNT + 1):
        ptr = u32(region, PTR_TABLE + index * 4)
        if ptr < BUS_BASE or ptr % 4 or ptr + COMMAND_BYTES > BUS_BASE + REGION_BYTES:
            raise InvalidSoundMatrix(f"profile {index}: invalid pointer 0x{ptr:08x}")
        row = [u32(region, ptr + column * 4)
               for column in range(COMMANDS_PER_PROFILE)]
        for column, command in enumerate(row):
            if command and command >> 8 not in VALID_PREFIXES:
                raise InvalidSoundMatrix(
                    f"profile {index} col {column}: non-sound value 0x{command:08x}")
        rows.append(row)
    return rows


def command_at(rows: list[list[int]], profile: int, column: int) -> int | None:
    """Return None for empty slots; reject out-of-range profile/column."""
    if not 1 <= profile <= PROFILE_COUNT or not 0 <= column < COMMANDS_PER_PROFILE:
        raise IndexError("profile must be 1..18 and column 0..10")
    value = rows[profile - 1][column]
    return value or None


def reverse_lookup(rows: list[list[int]], command: int) -> list[tuple[int, int]]:
    if not 0 < command <= 0xFFFFFFFF:
        return []
    return [(profile, column)
            for profile in range(1, PROFILE_COUNT + 1)
            for column in range(COMMANDS_PER_PROFILE)
            if command_at(rows, profile, column) == command]


def summarize(rows: list[list[int]]) -> dict:
    nonzero = [cmd for row in rows for cmd in row if cmd]
    family = Counter(f"0x{cmd >> 8:06x}" for cmd in nonzero)
    shared = Counter(nonzero)
    return {
        "profiles": len(rows), "columns": COMMANDS_PER_PROFILE,
        "nonzero_cells": len(nonzero),
        "empty_cells": PROFILE_COUNT * COMMANDS_PER_PROFILE - len(nonzero),
        "distinct_commands": len(shared),
        "commands_reused_across_cells": sum(1 for n in shared.values() if n > 1),
        "family_counts": dict(sorted(family.items())),
        "nonzero_by_profile": [sum(1 for cmd in row if cmd) for row in rows],
        "interpretation": "ROM-backed table selections; not proof of gameplay trigger or audio output",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="local vf2.zip or roms/vf2 directory")
    queries = parser.add_mutually_exclusive_group()
    queries.add_argument("--lookup", nargs=2, type=int, metavar=("PROFILE", "COLUMN"))
    queries.add_argument("--reverse", type=lambda s: int(s, 0), metavar="COMMAND_HEX")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    rows = decode_matrix(reconstruct_main_data_primary(args.rom))
    result = summarize(rows)
    if args.lookup:
        profile, column = args.lookup
        value = command_at(rows, profile, column)
        result["lookup"] = {"profile": profile, "column": column,
                            "command": f"0x{value:08x}" if value is not None else None}
    elif args.reverse is not None:
        result["reverse"] = {"command": f"0x{args.reverse:08x}",
                             "locations": reverse_lookup(rows, args.reverse)}
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print("VF2.1 Model 2A ROM sound selection matrix")
        for key in ("profiles", "columns", "nonzero_cells", "empty_cells",
                    "distinct_commands", "commands_reused_across_cells"):
            print(f"{key}: {result[key]}")
        if "lookup" in result:
            print("lookup:", result["lookup"])
        if "reverse" in result:
            print("reverse:", result["reverse"])
        print(result["interpretation"])


if __name__ == "__main__":
    main()
