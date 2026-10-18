#!/usr/bin/env python3
"""ROM-backed, non-executing i960 field cross-reference census.

Input: user's local vf2.zip. Never writes ROM bytes or derived artifacts.
A hit is a 4-byte-aligned *candidate*, not proven reachable code.
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from zipfile import ZipFile

PROGRAM_SIZE = 0x80000
MAINCPU_SIZE = 0x200000
# load32_word (not load32_byte), as in config/vf2_v22_roms.csv
PROGRAM_LANES = (
    ("epr-18385.12", 0x000000),
    ("epr-18386.13", 0x000002),
    ("epr-18387.14", 0x040000),
    ("epr-18388.15", 0x040002),
)
MEMORY_OPS = {
    0x80: ("ldob", 1, "read"),
    0x82: ("stob", 1, "write"),
    0x88: ("ldos", 2, "read"),
    0x8A: ("stos", 2, "write"),
    0xC0: ("ldib", 1, "read"),
    0xC2: ("stib", 1, "write"),
    0xC8: ("ldis", 2, "read"),
    0xCA: ("stis", 2, "write"),
}
FIELDS = {
    0x1B1: "fighter-type / character selector (NOT a sound event ID)",
    0x624: "fighter field read by sound dispatch (event meaning unverified)",
}


@dataclass(frozen=True)
class Site:
    pc: int
    mnemonic: str
    access: str
    width: int
    base_reg: int
    data_reg: int
    displacement: int
    preceding_copy: bool
    immediate_zero: bool


def reconstruct_program(rom_zip: Path) -> bytes:
    """Reproduce the supported manifest's LOAD32_WORD layout exactly."""
    region = bytearray(MAINCPU_SIZE)
    with ZipFile(rom_zip) as archive:
        available = set(archive.namelist())
        for name, offset in PROGRAM_LANES:
            if name not in available:
                raise ValueError(f"missing required CPU ROM: {name}")
            data = archive.read(name)
            if len(data) != 0x20000:
                raise ValueError(f"bad CPU ROM size for {name}: {len(data)}")
            region[offset:offset + len(data) * 2:4] = data[::2]
            region[offset + 1:offset + len(data) * 2:4] = data[1::2]
    return bytes(region[:PROGRAM_SIZE])


def decode_mema(word: int, fields: set[int]
                ) -> tuple[str, str, int, int, int, int] | None:
    """Direct base+12-bit-displacement MEMA only. Never guess MEMB."""
    opcode = (word >> 24) & 0xFF
    if opcode not in MEMORY_OPS or word & (1 << 12) or not word & (1 << 13):
        return None
    displacement = word & 0xFFF
    if displacement not in fields:
        return None
    mnemonic, width, access = MEMORY_OPS[opcode]
    return mnemonic, access, width, (word >> 14) & 31, (word >> 19) & 31, displacement


def scan(program: bytes, fields: set[int] | None = None) -> list[Site]:
    if fields is None:
        fields = set(FIELDS)
    sites: list[Site] = []
    for pc in range(0, len(program) - 3, 4):
        word = struct.unpack_from("<I", program, pc)[0]
        decoded = decode_mema(word, fields)
        if decoded is None:
            continue
        mnemonic, access, width, base, reg, displacement = decoded
        previous_copy = False
        if pc >= 4 and access == "write" and displacement == 0x1B1:
            before = struct.unpack_from("<I", program, pc - 4)[0]
            prev = decode_mema(before, {0x1B0})
            if prev is not None:
                _, prev_access, prev_width, prev_base, prev_reg, _ = prev
                previous_copy = (prev_access == "read" and prev_width == 1
                                 and prev_base == base and prev_reg == reg)
        # Two explicit ROM-observed zero paths. Not a generalized dataflow proof:
        # mov 0,r15 / stis r15,+624; mov 0,r3 / stob r3,+2a / stos r3,+624.
        zero = (
            (displacement == 0x624 and mnemonic == "stis" and reg == 15
             and pc >= 4 and struct.unpack_from("<I", program, pc - 4)[0] == 0x5C781E00)
            or
            (displacement == 0x624 and mnemonic == "stos" and reg == 3
             and base == 23 and pc >= 8
             and struct.unpack_from("<I", program, pc - 4)[0] == 0x821DE02A
             and struct.unpack_from("<I", program, pc - 8)[0] == 0x5C181E00)
        )
        sites.append(Site(pc, mnemonic, access, width, base, reg,
                          displacement, previous_copy, zero))
    return sites


def report(sites: list[Site]) -> dict:
    fields = []
    for offset, description in FIELDS.items():
        selected = [s for s in sites if s.displacement == offset]
        counts = Counter(f"{s.mnemonic}:{s.access}" for s in selected)
        fields.append({
            "offset": f"0x{offset:03x}",
            "meaning": description,
            "candidate_count": len(selected),
            "counts": dict(sorted(counts.items())),
            "immediate_previous_byte_copy_count": sum(s.preceding_copy for s in selected),
            "adjacent_zeroed_short_writer_count": sum(s.immediate_zero for s in selected),
            "sites": [{**asdict(s), "pc": f"0x{s.pc:05x}",
                       "displacement": f"0x{s.displacement:03x}"}
                      for s in selected],
        })
    return {"status": "static aligned MEMA candidates, NOT a runtime differential",
            "rom_data_committed": False, "fields": fields}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("rom_zip", type=Path, help="local vf2.zip, never committed")
    p.add_argument("--json", action="store_true", help="emit machine-readable site list")
    args = p.parse_args()
    data = report(scan(reconstruct_program(args.rom_zip)))
    if args.json:
        print(json.dumps(data, indent=2))
    else:
        print(data["status"])
        for field in data["fields"]:
            print(f"{field['offset']}: {field['meaning']}")
            print(f"  aligned candidates: {field['candidate_count']}; "
                  f"direct adjacent byte copies: {field['immediate_previous_byte_copy_count']}; "
                  f"adjacent-zero shorts: {field['adjacent_zeroed_short_writer_count']}")
            print("  " + ", ".join(f"{k}={v}" for k, v in field["counts"].items()))
            print("  writes: " + ", ".join(s["pc"] for s in field["sites"]
                                           if s["access"] == "write"))
        print("Next: check candidate PCs with vf2i960 disasm and a live vf2probe "
              "memory trace. Static hits cannot establish event provenance.")


if __name__ == "__main__":
    main()
