#!/usr/bin/env python3
"""Passive provenance for vf2probe --trace --memory-trace JSONL.

Never injects commands, changes machine state or claims audible output.
Source trace files, ROMs and snapshots must stay local.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

QUEUE_BEGIN = 0x00504020
QUEUE_END = QUEUE_BEGIN + 16 * 4
QUEUE_COUNT = 0x00504001
QUEUE_READ_CURSOR = 0x00504002
QUEUE_WRITE_CURSOR = 0x00504003
QUEUE_STAGE = 0x00504010
QUEUE_BYTE_CURSOR = 0x00504014
UART = 0x01C80000
RING_LABELS = {
    QUEUE_COUNT: "queue_count", QUEUE_READ_CURSOR: "read_cursor",
    QUEUE_WRITE_CURSOR: "write_cursor", QUEUE_BYTE_CURSOR: "byte_cursor",
}


def decode_sound_memory(record, fighter_bases):
    """Retain only validated sound writes; do not invent a full packet."""
    if record.get("kind") != "write":
        return None
    step, addr, size, raw = (record.get("step"), record.get("address"),
                             record.get("size"), record.get("bytes"))
    if (not isinstance(step, int) or isinstance(step, bool)
            or not isinstance(addr, int) or isinstance(addr, bool)
            or not isinstance(size, int) or isinstance(size, bool)
            or not 1 <= size <= 8 or not isinstance(raw, str)
            or len(raw) != size * 2):
        return None
    try:
        value = int.from_bytes(bytes.fromhex(raw), "little")
    except ValueError:
        return None
    if QUEUE_BEGIN <= addr < QUEUE_END and addr + size <= QUEUE_END:
        kind = ("queue_entry" if size == 4 and (addr - QUEUE_BEGIN) % 4 == 0
                else "queue_fragment")
    elif addr < QUEUE_END and addr + size > QUEUE_BEGIN:
        kind = "queue_boundary_overlap"
    elif addr in RING_LABELS:
        kind = RING_LABELS[addr]
    elif addr == QUEUE_STAGE:
        kind = "staged_command"
    elif addr == UART:
        kind = "uart_byte"
    else:
        kind = None
        for base in fighter_bases:
            if addr == base + 0x1B1:
                kind = "fighter_type"
                break
            if addr == base + 0x624:
                kind = "fighter_field_624"
                break
    if kind is None:
        return None
    return {"step": step, "pc": None, "address": addr,
            "kind": kind, "width": size, "value": value}


def parse_events(lines, fighter_bases=()):
    """Stream vf2probe events; join only the identical integer step.

    Memory-observer callbacks occur before the step trace callback
    for an instruction. Keep only matching sound records and a single
    pending-step group instead of indexing every instruction in RAM.
    Also handles step-first fixtures without backfilling neighbors.
    """
    out, pending = [], []
    last_step = last_pc = None
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
        if record.get("type") == "step":
            pc = record.get("ip_before")
            pc = pc if isinstance(pc, int) and not isinstance(pc, bool) else None
            for event in pending:
                if event["step"] == step:
                    event["pc"] = pc
                out.append(event)
            pending.clear()
            last_step, last_pc = step, pc
        elif record.get("type") == "memory":
            event = decode_sound_memory(record, fighter_bases)
            if event is None:
                continue
            if last_step == step:
                event["pc"] = last_pc
                out.append(event)
            else:
                if pending and pending[-1]["step"] != step:
                    out.extend(pending)
                    pending.clear()
                pending.append(event)
    out.extend(pending)
    return sorted(out, key=lambda event: (event["step"], event["address"]))


def summarize(events):
    counts = Counter(e["kind"] for e in events)
    writers = Counter(f"0x{e['pc']:05x}" if e["pc"] is not None else "unknown"
                      for e in events if e["kind"] == "queue_entry")
    queue = [e for e in events if e["kind"] == "queue_entry"]
    uart = [e for e in events if e["kind"] == "uart_byte"]
    return {
        "trace_kind": "passive observer; not synthetic injection",
        "gameplay_audio_proven": False,
        "counts": dict(sorted(counts.items())),
        "queue_writer_pcs": dict(sorted(writers.items())),
        "first_queue_write": queue[0] if queue else None,
        "first_uart_write": uart[0] if uart else None,
        "observed_events": events,
        "limits": "PC unknown without paired --trace; queue may be boot/setup; SCSP/audibility not inferred",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="local vf2probe JSONL trace")
    parser.add_argument("--fighter-base", action="append", type=lambda s: int(s, 0), default=[],
                        help="actual fighter base from snapshot; repeatable")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    with args.trace.open(encoding="utf-8", errors="replace") as stream:
        report = summarize(parse_events(stream, args.fighter_base))
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print("Passive sound trace (no gameplay or audible-SFX proof)")
        print("Event counts:", report["counts"])
        print("Queue writer PCs:", report["queue_writer_pcs"])
        for name in ("first_queue_write", "first_uart_write"):
            print(name, report[name])
        print(report["limits"])


if __name__ == "__main__":
    main()
