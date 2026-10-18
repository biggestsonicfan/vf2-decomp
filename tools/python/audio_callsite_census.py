#!/usr/bin/env python3
"""i960 sound direct-call census on user-supplied local vf2.zip.

Static CTRL call/bal only; CALLX, reachability and code/data remain unknown.
Never writes ROM-derived bytes to disk or commits proprietary assets.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from audio_field_xrefs import reconstruct_program

TARGETS = {
    0x437D0: "uart_drain_entry",
    0x43800: "uart_drainer_fallthrough",
    0x43888: "queue_filer",
    0x438EC: "queue_common_tail",
    0x43950: "fighter_sound_driver",
    0x439AC: "command_dedup",
    0x43ABC: "per_frame_sound_flusher",
    0x18A54: "model2a_sound_table_dispatch",
}


def decode_direct_call(program: bytes, pc: int) -> tuple[str, int] | None:
    """Same CTRL signed displacement convention as src/i960/decoder.c."""
    if pc < 0 or pc % 4 != 0 or pc + 4 > len(program):
        return None
    word = struct.unpack_from("<I", program, pc)[0]
    opcode = word >> 24
    if opcode not in (0x09, 0x0B):
        return None
    displacement = word & 0x00FFFFFC
    if displacement & 0x00800000:
        displacement -= 0x01000000
    return ("call" if opcode == 0x09 else "bal",
            (pc + displacement) & 0xFFFFFFFF)


def census(program: bytes) -> dict:
    candidates = {name: [] for name in TARGETS.values()}
    for pc in range(0, len(program) - 3, 4):
        decoded = decode_direct_call(program, pc)
        if decoded is None:
            continue
        op, target = decoded
        if target in TARGETS:
            candidates[TARGETS[target]].append({"pc": pc, "operation": op})
    pairs = []
    for entry in candidates["command_dedup"]:
        call = decode_direct_call(program, entry["pc"] + 4)
        if call is not None and call[1] == 0x43888:
            pairs.append({"dedup_pc": entry["pc"], "filer_pc": entry["pc"] + 4})
    return {
        "status": "aligned ROM candidates ONLY: neither reachability nor gameplay audio proven",
        "callers": {
            name: {
                "count": len(entries),
                "sites": [{"pc": f"0x{x['pc']:05x}", "op": x["operation"]}
                          for x in entries],
            } for name, entries in candidates.items()
        },
        "adjacent_dedup_then_filer": [
            {"dedup_pc": f"0x{x['dedup_pc']:05x}",
             "filer_pc": f"0x{x['filer_pc']:05x}"} for x in pairs
        ],
        "model2a_dispatch_two_direct_calls": [
            f"0x{x['pc']:05x}" for x in candidates["model2a_sound_table_dispatch"]
        ],
        "limits": ["no CALLX/indirect edges", "no instruction/data proof",
                   "no path-condition or natural-contact proof"],
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("rom_zip", type=Path)
    p.add_argument("--json", action="store_true")
    args = p.parse_args()
    result = census(reconstruct_program(args.rom_zip))
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(result["status"])
        for name, entry in result["callers"].items():
            print(f"{name}: {entry['count']} direct CTRL sites")
        print("Model 2A dispatcher direct callsites:",
              ", ".join(result["model2a_dispatch_two_direct_calls"]))
        print("Adjacent dedup->filer:", len(result["adjacent_dedup_then_filer"]))


if __name__ == "__main__":
    main()
