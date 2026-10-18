#!/usr/bin/env python3
"""Opcode-anchored, passive ROM selection -> queue-write witness for VF2.1.

An observed command selection is insufficient to claim delivery. This
observer requires the actual mov/call/return flow to dedup + filer and a
full matching ring-entry write inside that filer invocation. It cannot
establish natural gameplay or 68000/SCSP side effects without additional
replay evidence. Never changes the emulator or writes ROM-derived content.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

from audio_command_matrix import reconstruct_main_data_primary
from audio_field_xrefs import reconstruct_program
from audio_selector_provenance import (assert_selector_opcodes,
                                        parse_observed_selections, word)
from audio_queue_provenance import decode_memory

LOAD_CMD = 0x18A98
MOV_CMD = 0x18AB8
CALL_DEDUP = 0x18ABC
ENTRY_DEDUP = 0x439AC
CALL_FILER = 0x18AC0
ENTRY_FILER = 0x43888
AFTER_FILER = 0x18AC4


def signed_target(pc: int, opcode: int) -> int:
    offset = opcode & 0x00FFFFFC
    if offset & 0x00800000:
        offset -= 0x01000000
    return (pc + offset) & 0xFFFFFFFF


def verify_fighter_delivery_opcodes(rom: bytes) -> dict:
    """Fail closed if original direct calls or r5->g0 move change.

    The i960 register-format mov opcode 0x5cc has destination bits
    23:19 and src1 bits 4:0. 0x5c801605 is mov r5, g0.
    """
    assert_selector_opcodes(rom)
    mov = word(rom, MOV_CMD)
    if mov != 0x5C801605:
        raise ValueError(f'changed r5->g0 instruction at {MOV_CMD:#x}')
    calls = ((CALL_DEDUP, ENTRY_DEDUP), (CALL_FILER, ENTRY_FILER))
    for pc, expected in calls:
        op = word(rom, pc)
        if op >> 24 != 9 or signed_target(pc, op) != expected:
            raise ValueError(f'changed direct call {pc:#x} -> {expected:#x}')
    return {'selected_command_load': f'0x{LOAD_CMD:05x}',
            'mov_selected_to_g0': f'0x{MOV_CMD:05x}',
            'dedup_call': f'0x{CALL_DEDUP:05x}',
            'queue_filer_call': f'0x{CALL_FILER:05x}',
            'evidence': 'ROM direct call and register-move opcodes; not gameplay'}


def witness_delivery(lines, selections: list[dict], *, max_window=1024,
                     max_witnesses=64) -> dict:
    """Join only exact-step memory writes to observed i960 call/return path.

    Accepts selections verified by parse_observed_selections on the SAME
    trace. Discontinuous, reordered or missing step records invalidate an
    active witness; no time-only matching or cross-caller reuse.
    """
    valid = {}
    for selection in selections:
        if (selection.get('route') == 'fighter_driver' and
                isinstance(selection.get('step'), int) and
                isinstance(selection.get('command'), int) and
                selection['command'] > 0):
            valid[selection['step']] = selection

    stats = Counter()
    witnesses = []
    active = None
    pending = []
    last_step = last_after = None

    def finish(reason: str, *, matched: bool = False) -> None:
        nonlocal active
        if active is None:
            return
        stats[reason] += 1
        if len(witnesses) < max_witnesses:
            witnesses.append({
                'selector_step': active['selection']['step'],
                'selected_command': active['selection']['command_hex'],
                'dedup_entered': active['dedup_entered'],
                'filer_entered': active['filer_entered'],
                'ring_writes': active['ring_writes'],
                'matching_complete_ring_write': matched,
                'result': reason,
                'gameplay_or_SCSP_proven': False,
            })
        active = None

    for line in lines:
        if not line.lstrip().startswith('{'):
            continue
        try:
            e = json.loads(line)
        except (ValueError, TypeError):
            continue
        if not isinstance(e, dict):
            continue
        step = e.get('step')
        if not isinstance(step, int) or isinstance(step, bool) or step < 0:
            continue
        if e.get('type') == 'memory':
            decoded = decode_memory(e, ())
            if decoded and decoded['kind'] in ('queue_entry', 'queue_fragment',
                                              'queue_boundary_overlap'):
                pending.append(decoded)
                if len(pending) > 32:
                    # Cannot trust a truncated single-instruction write group.
                    stats['pending_memory_overflow'] += 1
                    pending.clear()
                    finish('incomplete_trace')
            continue
        if e.get('type') != 'step':
            continue
        ip, after = e.get('ip_before'), e.get('ip_after')
        if (not isinstance(ip, int) or isinstance(ip, bool) or
                not isinstance(after, int) or isinstance(after, bool)):
            pending.clear()
            finish('incomplete_trace')
            continue
        if last_step is not None and (step != last_step + 1 or ip != last_after):
            finish('discontinuous_trace')
            stats['trace_discontinuities'] += 1
        mem = [m for m in pending if m['step'] == step]
        stale = sum(m['step'] != step for m in pending)
        if stale:
            stats['unpaired_memory_records'] += stale
            finish('incomplete_trace')
        pending.clear()
        last_step, last_after = step, after

        if active and step - active['selection']['step'] > max_window:
            finish('timed_out')

        if ip == LOAD_CMD and step in valid:
            if active:
                finish('interrupted_by_new_selection')
            active = {'selection': valid[step], 'phase': 'await_mov',
                      'dedup_entered': False, 'filer_entered': False,
                      'ring_writes': []}
            stats['verified_selections_seen'] += 1
            continue
        if active is None:
            continue
        phase = active['phase']
        if phase == 'await_mov':
            if ip == MOV_CMD and after == CALL_DEDUP:
                active['phase'] = 'await_dedup'
                stats['mov_selected_to_g0_observed'] += 1
            elif ip == CALL_FILER or (ip > MOV_CMD and ip < 0x18AC8):
                finish('skipped_command_move')
            continue
        if phase == 'await_dedup':
            if ip == CALL_DEDUP and after == ENTRY_DEDUP:
                active['phase'] = 'inside_dedup'
                active['dedup_entered'] = True
                stats['dedup_call_observed'] += 1
            else:
                finish('unexpected_after_mov')
            continue
        if phase == 'inside_dedup':
            if after == CALL_FILER:
                active['phase'] = 'await_filer'
            continue
        if phase == 'await_filer':
            if ip == CALL_FILER and after == ENTRY_FILER:
                active['phase'] = 'inside_filer'
                active['filer_entered'] = True
                stats['filer_call_observed'] += 1
            else:
                finish('unexpected_after_dedup')
            continue
        if phase == 'inside_filer':
            for write in mem:
                if write['kind'] == 'queue_entry':
                    active['ring_writes'].append({
                        'step': step, 'pc': ip, 'value': f"0x{write['value']:08x}",
                        'matches_selection': write['value'] == active['selection']['command']})
                    stats['complete_ring_writes_inside_filer'] += 1
                else:
                    stats['partial_ring_writes_inside_filer'] += 1
            if after == AFTER_FILER:
                matched = any(w['matches_selection'] for w in active['ring_writes'])
                finish('command_delivered_to_ring' if matched else
                       'filer_returned_without_matching_ring_word', matched=matched)
            continue
    if active:
        finish('trace_ended_before_filer_return')
    if pending:
        stats['unpaired_memory_records'] += len(pending)
    return {'status': 'passive ROM-selector-to-ring execution witness',
            'counts': dict(sorted(stats.items())), 'witnesses': witnesses,
            'natural_combat_audio_proven': False,
            'scope': 'Only fighter_driver, exact word match; queue not UART/68000/SCSP'}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path, help='vf2.zip or roms/vf2 locally')
    parser.add_argument('trace', type=Path, help='same vf2probe --trace --memory-trace JSONL')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    proof = verify_fighter_delivery_opcodes(reconstruct_program(args.rom))
    main_data = reconstruct_main_data_primary(args.rom)
    with args.trace.open(encoding='utf-8', errors='replace') as file:
        selected = parse_observed_selections(file, main_data)
    with args.trace.open(encoding='utf-8', errors='replace') as file:
        delivery = witness_delivery(file, selected['selections'])
    result = {'opcode_proof': proof,
              'observed_selection_counts': selected['counts'],
              'delivery': delivery}
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print('ROM-anchored selector -> dedup -> filer witness')
        print('Observed selection counts:', selected['counts'])
        print('Delivery counts:', delivery['counts'])
        for row in delivery['witnesses']:
            print(row)
        print('Natural gameplay and SCSP audio remain unproven')


if __name__ == '__main__':
    main()
