#!/usr/bin/env python3
"""Audit oracle-frame sound evidence without claiming a natural combat hit.

Reads newline-delimited observations from vf2_host --audio-evidence-jsonl.
A valid report can identify promising *frames* for the independent original
i960 opcode/selector trace. It cannot attest event provenance or true
68000 IRQ/DSP/audio timing; a JSONL file is not cryptographic evidence.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path

HEX = re.compile(r"[0-9a-fA-F]*\Z")
CAP = 48


def _integer(event: dict, name: str, maximum: int | None = None) -> int:
    value = event.get(name)
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ValueError(f"{name}: expected non-negative integer")
    if maximum is not None and value > maximum:
        raise ValueError(f"{name}: value exceeds maximum {maximum}")
    return value


def audit_observations(lines, *, max_candidates: int = 64) -> dict:
    if not isinstance(max_candidates, int) or max_candidates < 0:
        raise ValueError("max_candidates must be nonnegative")
    frames = 0
    last_frame = -1
    count = Counter()
    candidates = []
    original_writers = []
    original_targets = []
    observed_bytes = 0
    for line_number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            event = json.loads(line)
        except (ValueError, TypeError) as exc:
            raise ValueError(f"line {line_number}: invalid JSON") from exc
        if not isinstance(event, dict) or event.get("type") != "vf2_audio_frame":
            raise ValueError(f"line {line_number}: unsupported record")
        if event.get("origin") != "oracle_frame_shadow_uart_pump":
            raise ValueError(f"line {line_number}: not an original i960 oracle frame")
        frame = _integer(event, "frame")
        if frame != last_frame + 1:
            raise ValueError(f"line {line_number}: discontinuous frame number")
        last_frame = frame
        frames += 1
        _integer(event, "input", 0xFFFFFFFF)
        before = _integer(event, "queue_before_step", 255)
        after = _integer(event, "queue_after_step", 255)
        original_writes = 0
        writer_pc_proven = False
        first_pc = 0
        if ("original_i960_ring_writes" not in event and
            ("original_i960_first_ring_slot" in event or
             "original_i960_first_ring_word" in event or
             "original_i960_first_writer_pc" in event or
             "original_i960_writer_pc_proven" in event)):
            raise ValueError(f"line {line_number}: original i960 evidence without writes")
        if "original_i960_ring_writes" in event:
            original_writes = _integer(event, "original_i960_ring_writes")
            first_slot = _integer(event, "original_i960_first_ring_slot")
            first_word = _integer(event, "original_i960_first_ring_word", 0xFFFFFFFF)
            writer_pc_proven = event.get("original_i960_writer_pc_proven")
            if not isinstance(writer_pc_proven, bool):
                raise ValueError(f"line {line_number}: invalid original i960 PC claim")
            first_pc = _integer(
                event, "original_i960_first_writer_pc", 0xFFFFFFFF
            ) if "original_i960_first_writer_pc" in event else 0
            if ((original_writes == 0 and
                 (first_slot != 0 or first_word != 0 or
                  first_pc != 0 or writer_pc_proven)) or
                    (original_writes > 0 and
                     (not 0x00504020 <= first_slot < 0x00504060 or
                      first_slot % 4 != 0)) or
                    (writer_pc_proven and
                     (original_writes == 0 or first_pc == 0 or
                      first_pc % 4 != 0)) or
                    (not writer_pc_proven and first_pc != 0)):
                raise ValueError(f"line {line_number}: invalid original i960 queue writer")
        # A PC-filtered checkpoint may target a LATER sound producer
        # in the same frame (after e.g. 0x100 startup queue clears).
        # Preserve both original-first and selected-target witnesses.
        if "original_i960_target_writer_observed" in event:
            target_found = event["original_i960_target_writer_observed"]
            if not isinstance(target_found, bool):
                raise ValueError(f"line {line_number}: target writer flag invalid")
            target_pc = _integer(event, "original_i960_target_writer_pc",
                                 0xFFFFFFFF)
            target_slot = _integer(event, "original_i960_target_slot",
                                   0xFFFFFFFF)
            target_word = _integer(event, "original_i960_target_word",
                                   0xFFFFFFFF)
            if target_found:
                if (original_writes == 0 or target_pc % 4 != 0 or
                    not 0x00504020 <= target_slot < 0x00504060 or
                    target_slot % 4 != 0):
                    raise ValueError(f"line {line_number}: invalid selected original writer")
            elif target_pc != 0 or target_slot != 0 or target_word != 0:
                raise ValueError(f"line {line_number}: unobserved target payload")
        forwarded = _integer(event, "bytes_moved", CAP)
        captured = _integer(event, "uart_count", CAP)
        truncated = _integer(event, "uart_truncated")
        hexed = event.get("uart_forwarded_hex")
        if (not isinstance(hexed, str) or len(hexed) != 2 * captured
                or HEX.fullmatch(hexed) is None):
            raise ValueError(f"line {line_number}: UART bytes invalid")
        if captured + truncated != forwarded:
            raise ValueError(f"line {line_number}: captured bytes disagree with transport")
        filed = _integer(event, "entries_filed", 255)
        dispatched = _integer(event, "entries_dispatched", 255)
        refused = _integer(event, "entries_refused", 255)
        keyons = _integer(event, "d92_keyons_retired", 255)
        if dispatched + refused != filed or keyons > dispatched:
            raise ValueError(f"line {line_number}: dispatcher accounting mismatch")
        pcm_frames = _integer(event, "pcm_frames")
        nonzero = _integer(event, "pcm_nonzero_frames")
        peak = _integer(event, "pcm_peak", 32768)
        if nonzero > pcm_frames or (pcm_frames == 0 and peak != 0):
            raise ValueError(f"line {line_number}: impossible PCM summary")
        for attr in ("pump_status", "drain_status", "stream_status",
                     "voice_status", "render_status"):
            if not isinstance(event.get(attr), int) or isinstance(event[attr], bool):
                raise ValueError(f"line {line_number}: status {attr} not integer")
        # These explicit false assertions are deliberately never promoted by
        # this parser; a witness requires an independently captured PC trace.
        for claim in ("natural_fighter_hit_proven", "original_uart_irq_proven",
                      "hardware_pcm_proven"):
            if event.get(claim) is not False:
                raise ValueError(f"line {line_number}: unsupported truth claim {claim}")
        observed_bytes += captured
        count["frames_with_uart"] += int(captured > 0)
        count["frames_with_original_i960_ring_writer"] += int(original_writes > 0)
        count["frames_with_original_i960_writer_pc"] += int(writer_pc_proven)
        if event.get("original_i960_target_writer_observed") is True:
            count["frames_with_selected_i960_writer"] += 1
        if original_writes > 0 and len(original_writers) < max_candidates:
            original_writers.append({
                "frame": frame,
                "input": event["input"],
                "ring_writes": original_writes,
                "first_slot": f"0x{first_slot:08x}",
                "first_command": f"0x{first_word:08x}",
                "original_instruction_ip": (
                    f"0x{first_pc:08x}" if writer_pc_proven else None
                ),
                "uart_forwarded_hex": hexed,
                "scope": ("original i960 bus WRITE and pre-step instruction PC; "
                          "neither original ISR timing nor fighter hit proven"),
            })
        if event.get("original_i960_target_writer_observed") is True and len(original_targets) < max_candidates:
            original_targets.append({
                "frame": frame,
                "input": event["input"],
                "pc": f"0x{target_pc:08x}",
                "slot": f"0x{target_slot:08x}",
                "word": f"0x{target_word:08x}",
                "scope": "original i960 selected queue write, not a fighter-hit cause",
            })
        count["frames_with_d92_keyon"] += int(keyons > 0)
        count["frames_with_nonzero_pcm"] += int(nonzero > 0)
        count["frames_with_i960_queue_growth"] += int(after > before)
        count["frames_with_truncation"] += int(truncated > 0)
        if after > before and forwarded > 0:
            count["temporal_queue_growth_uart_candidates"] += 1
            if len(candidates) < max_candidates:
                candidates.append({
                    "frame": frame,
                    "input": event["input"],
                    "queue_before": before,
                    "queue_after": after,
                    "uart_forwarded_hex": hexed,
                    "d92_keyons_retired": keyons,
                    "pcm_nonzero_frames": nonzero,
                    "reason": "queue growth and shadow UART in same frame; NOT cause of hit",
                })
    return {
        "status": "passive_oracle_frame_candidates_only",
        "total_frames": frames,
        "uart_bytes_observed": observed_bytes,
        "counts": dict(sorted(count.items())),
        "candidates": candidates,
        "candidates_truncated": count["temporal_queue_growth_uart_candidates"] > len(candidates),
        "original_i960_ring_writers": original_writers,
        "original_i960_selected_writers": original_targets,
        "original_writer_samples_truncated":
            count["frames_with_original_i960_ring_writer"] > len(original_writers),
        "gates": {
            "frame_progression_observed": frames > 0,
            "original_i960_ring_write_observed":
                bool(count["frames_with_original_i960_ring_writer"]),
            "frame_local_queue_to_shadow_uart_candidate": bool(count["temporal_queue_growth_uart_candidates"]),
            "original_producer_pc_verified":
                bool(count["frames_with_original_i960_writer_pc"]),
            "original_isr_to_m68k_link_verified": False,
            "natural_fighter_hit_proven": False,
            "hardware_audio_verified": False,
        },
        "next": ("Replay the captured pre-event snapshot, verify the recorded "
                 "original i960 instruction IP and queue word with an "
                 "independent oracle trace; then correlate vector-15 UART "
                 "and original 68000 L3. Neither the write nor PCM "
                 "establishes a natural fighter hit."),
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("jsonl", type=Path)
    p.add_argument("--max-candidates", type=int, default=64)
    args = p.parse_args()
    with args.jsonl.open(encoding="utf-8") as records:
        report = audit_observations(records, max_candidates=args.max_candidates)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
