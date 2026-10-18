#!/usr/bin/env python3
"""ROM-anchored i960 audio-selector provenance and passive trace attestation.

The row index is r3; the column is fighter-type in r4; the selected
command is r5. Static opcode evidence != naturally audible gameplay.
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path

from audio_command_matrix import (
    PTR_TABLE, PROFILE_COUNT, COMMANDS_PER_PROFILE,
    decode_matrix, reconstruct_main_data_primary, u32,
)
from audio_field_xrefs import reconstruct_program
from audio_queue_provenance import parse_events

# (fighter-type load, pointer lookup, command load, fighter base, row pointer)
ROUTES = {
    "fighter_driver": (0x18A8C, 0x18A90, 0x18A98, 23, 3),
    "round_selector": (0x10258, 0x1027C, 0x10284, 7, 15),
    "state_selector": (0x1B070, 0x1B074, 0x1B07C, 23, 15),
}


def word(image: bytes, pc: int) -> int:
    if pc < 0 or pc % 4 or pc + 4 > len(image):
        raise ValueError(f"invalid opcode PC 0x{pc:05x}")
    return struct.unpack_from("<I", image, pc)[0]


def assert_selector_opcodes(image: bytes) -> list[dict]:
    """Fail closed on changed i960 MEMA/MEMB opcode addressing forms."""
    proof = []
    for label, (char_pc, table_pc, command_pc, fighter_reg, rowptr_reg) in ROUTES.items():
        read_char = word(image, char_pc)
        valid_char = (
            read_char >> 24 == 0x80
            and not read_char & 0x1000
            and bool(read_char & 0x2000)
            and (read_char & 0xFFF) == 0x1B1
            and (read_char >> 14) & 31 == fighter_reg
            and (read_char >> 19) & 31 == 4
        )
        select_row = word(image, table_pc)
        valid_row = (
            select_row >> 24 == 0x90
            and bool(select_row & 0x1000)
            and (select_row >> 10) & 15 == 0x0E
            and (select_row >> 7) & 7 == 2
            and select_row & 31 == 3
            and (select_row >> 19) & 31 == rowptr_reg
            and not select_row & 0x60
            and word(image, table_pc + 4) == PTR_TABLE
        )
        select_command = word(image, command_pc)
        valid_command = (
            select_command >> 24 == 0x90
            and (select_command >> 10) & 15 == 7
            and (select_command >> 7) & 7 == 2
            and select_command & 31 == 4
            and (select_command >> 14) & 31 == rowptr_reg
            and (select_command >> 19) & 31 == 5
            and not select_command & 0x60
        )
        if not (valid_char and valid_row and valid_command):
            raise ValueError(f"{label}: unverified character/row/command opcode structure")
        proof.append({
            "route": label, "fighter_type_load": f"0x{char_pc:05x}",
            "row_pointer_load": f"0x{table_pc:05x}",
            "command_load": f"0x{command_pc:05x}",
            "row_index_register": "r3", "column_index_register": "r4",
            "command_result_register": "r5",
            "fighter_base_register": f"r{fighter_reg}" if fighter_reg < 16
            else f"g{fighter_reg - 16}",
        })
    return proof


def parse_observed_selections(
    lines, main_data: bytes, *, max_samples=48, max_selections=256,
) -> dict:
    """Verify each executed read at its exact PC and memory-step boundary.

    Every row pointer and command read must match the local ROM.
    Output retention is capped; the total matched count is not capped.
    """
    matrix = decode_matrix(main_data)
    counts = Counter()
    samples = []
    selections = []
    active = {name: None for name in ROUTES}
    pending = []
    last_step = None
    last_after = None
    for line in lines:
        if not line.lstrip().startswith("{"):
            continue
        try:
            record = json.loads(line)
        except (ValueError, TypeError):
            continue
        if not isinstance(record, dict):
            continue
        step = record.get("step")
        if not isinstance(step, int) or isinstance(step, bool):
            continue
        if record.get("type") == "memory":
            if record.get("kind") != "read":
                continue
            addr, size, raw = record.get("address"), record.get("size"), record.get("bytes")
            if (not isinstance(addr, int) or not isinstance(size, int)
                    or size not in (1, 4) or not isinstance(raw, str)
                    or len(raw) != size * 2):
                continue
            try:
                value = int.from_bytes(bytes.fromhex(raw), "little")
            except ValueError:
                continue
            pending.append({"step": step, "address": addr, "size": size, "value": value})
            if len(pending) > 32:
                pending = pending[-16:]
                counts["pending_overflow"] += 1
            continue
        if record.get("type") != "step":
            continue
        pc, after = record.get("ip_before"), record.get("ip_after")
        if not isinstance(pc, int) or not isinstance(after, int):
            continue
        if last_step is not None and (step != last_step + 1 or pc != last_after):
            active = {name: None for name in ROUTES}
            counts["trace_discontinuities"] += 1
        readings = [r for r in pending if r["step"] == step]
        pending = [r for r in pending if r["step"] > step]
        last_step = step
        last_after = after
        for route, (char_pc, table_pc, command_pc, _base, _pointer) in ROUTES.items():
            state = active[route]
            if state and (
                pc < char_pc or pc > command_pc
                or step - state["char_step"] > 32
            ):
                active[route] = None
                state = None
            if pc == char_pc:
                chars = [r["value"] for r in readings if r["size"] == 1]
                if len(chars) == 1 and 0 <= chars[0] < COMMANDS_PER_PROFILE:
                    active[route] = {"column": chars[0], "char_step": step}
                    counts["character_index_observed"] += 1
                else:
                    active[route] = None
                    counts["character_index_unverified"] += 1
            elif pc == table_pc and state and state.get("pointer") is None:
                valid = []
                for r in readings:
                    if r["size"] != 4 or (r["address"] - PTR_TABLE) % 4:
                        continue
                    row = (r["address"] - PTR_TABLE) // 4
                    if not 1 <= row <= PROFILE_COUNT:
                        continue
                    if r["value"] == u32(main_data, PTR_TABLE + 4 * row):
                        valid.append((row, r["value"]))
                if len(valid) == 1:
                    state["row"], state["pointer"] = valid[0]
                    counts["row_pointer_observed"] += 1
                else:
                    active[route] = None
                    counts["row_pointer_unverified"] += 1
            elif pc == command_pc and state:
                row, column = state.get("row"), state["column"]
                expected = matrix[row - 1][column] if row is not None else None
                valid = [
                    r for r in readings
                    if r["size"] == 4
                    and r["address"] == state.get("pointer", -1) + column * 4
                ]
                if len(valid) == 1 and valid[0]["value"] == expected:
                    event = {
                        "route": route, "step": step, "row": row,
                        "fighter_type": column, "command": expected,
                        "command_hex": f"0x{expected:08x}",
                        "interpretation": "original ROM selection observed; not gameplay audio",
                    }
                    if len(selections) < max_selections:
                        selections.append(event)
                    else:
                        counts["selections_exceed_report_cap"] += 1
                    if len(samples) < max_samples:
                        samples.append(event)
                    counts["rom_exact_selections"] += 1
                else:
                    counts["command_read_mismatch_or_incomplete"] += 1
                active[route] = None
    return {
        "status": "passive, opcode-verified selection evidence",
        "counts": dict(sorted(counts.items())),
        "samples": samples, "selections": selections,
        "gameplay_audio_proven": False,
    }


def attribute_queue_candidates(
    selections: list[dict], sound_writes: list[dict], *, max_instructions=256,
) -> list[dict]:
    """Matching later queue words are temporal candidates, NOT causal proof."""
    queue = sorted((e for e in sound_writes if e["kind"] == "queue_entry"),
                   key=lambda e: e["step"])
    used = set()
    out = []
    for chosen in selections:
        hit = None
        for i, event in enumerate(queue):
            if i in used or not (
                chosen["step"] < event["step"] <= chosen["step"] + max_instructions
            ):
                continue
            if chosen["command"] and chosen["command"] == event["value"]:
                hit = event
                used.add(i)
                break
        out.append({
            **chosen,
            "matching_queue_write_step": hit["step"] if hit else None,
            "matching_queue_writer_pc": hit["pc"] if hit else None,
            "queue_relationship": "temporal candidate, not causal proof"
            if hit else "not observed",
        })
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="user-local vf2.zip or ROM directory")
    parser.add_argument("--trace", type=Path, help="vf2probe --trace --memory-trace JSONL")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    main_rom = reconstruct_program(args.rom)
    main_data = reconstruct_main_data_primary(args.rom)
    result = {
        "static_opcode_proof": assert_selector_opcodes(main_rom),
        "selection_matrix_size": [PROFILE_COUNT, COMMANDS_PER_PROFILE],
    }
    if args.trace:
        with args.trace.open("r", encoding="utf8", errors="replace") as stream:
            observed = parse_observed_selections(stream, main_data)
        with args.trace.open("r", encoding="utf8", errors="replace") as stream:
            writes = parse_events(stream)
        result["observed"] = {
            key: value for key, value in observed.items() if key != "selections"
        }
        result["queue_candidates"] = attribute_queue_candidates(
            observed["selections"], writes
        )[:48]
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        for item in result["static_opcode_proof"]:
            print(item)
        if "observed" in result:
            print("live selection counts:", result["observed"]["counts"])
            print("queue candidates:", result["queue_candidates"])
        print("Static opcode proof != sound played by natural gameplay")


if __name__ == "__main__":
    main()
