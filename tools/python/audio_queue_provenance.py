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


def parse_events(lines, fighter_bases=()):
    """Join memory with instruction PC on SAME step; missing PC stays unknown.

    vf2probe assigns memory step cpu.executed_instructions+1; its
    instruction trace has the same event.step. Do not use adjacent PCs.
    """
    steps = {}
    memory = []
    for line in lines:
        if not line.lstrip().startswith("{"):
            continue
        try:
            record = json.loads(line)
        except (ValueError, TypeError):
            continue
        if not isinstance(record, dict) or not isinstance(record.get("step"), int):
            continue
        if record.get("type") == "step" and isinstance(record.get("ip_before"), int):
            steps[record["step"]] = record["ip_before"]
        elif record.get("type") == "memory" and record.get("kind") == "write":
            memory.append(record)

    out = []
    for record in memory:
        addr, size = record.get("address"), record.get("size")
        if not isinstance(addr, int) or not isinstance(size, int) or not 1 <= size <= 8:
            continue
        raw = record.get("bytes")
        if not isinstance(raw, str) or len(raw) != 2 * size:
            continue
        try:
            data = bytes.fromhex(raw)
        except ValueError:
            continue
        label = None
        if addr < QUEUE_END and addr + size > QUEUE_BEGIN:
            label = "queue_entry"
        elif addr in RING_LABELS:
            label = RING_LABELS[addr]
        elif addr == QUEUE_STAGE:
            label = "staged_command"
        elif addr == UART:
            label = "uart_byte"
        else:
            for base in fighter_bases:
                if addr == base + 0x1B1:
                    label = "fighter_type"
                    break
                if addr == base + 0x624:
                    label = "fighter_field_624"
                    break
        if label is None:
            continue
        pc = steps.get(record["step"])
        out.append({
            "step": record["step"], "pc": pc, "address": addr,
            "kind": label, "width": size, "value": int.from_bytes(data, "little"),
        })
    return sorted(out, key=lambda item: (item["step"], item["address"]))


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
