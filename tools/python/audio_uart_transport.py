#!/usr/bin/env python3
"""Passive ROM-anchored i960 queue -> vector-15 UART transport witness.

A witness proves bytes written to the modeled UART slot, NOT their reception
by the 68000 or any SCSP voice. Never injects events or stores ROM content.
"""
from __future__ import annotations
import argparse
import json
from collections import Counter
from pathlib import Path
from audio_field_xrefs import reconstruct_program
from audio_selector_provenance import word, parse_observed_selections
from audio_command_matrix import reconstruct_main_data_primary
from audio_selector_delivery import verify_fighter_delivery_opcodes, witness_delivery

QUEUE_LO, QUEUE_HI = 0x504020, 0x504060
STAGE, CURSOR, UART = 0x504010, 0x504014, 0x1C80000
IRQ_CALL, DRAIN_ENTRY, IRQ_RETURN = 0xE24, 0x437D0, 0xE28
POP_READ, STAGE_WRITE = 0x4382C, 0x43834
BYTE_READ, UART_WRITE, CURSOR_WRITE, DRAIN_RET = 0x4385C, 0x43868, 0x43870, 0x43884
QUEUE_WRITE = 0x43924
# ROM instructions and their immediate-address extension words, all i960 LE.
ROM_WORDS = {
    0xE24: 0x090429AC, 0x4382C: 0x90203903, 0x43830: QUEUE_LO,
    0x43834: 0x92203000, 0x43838: STAGE,
    0x4385C: 0x8020DC05, 0x43860: 0x8C183000,
    0x43864: UART, 0x43868: 0x8A20D000,
    0x43870: 0xC2283000, 0x43874: CURSOR,
    0x43884: 0x0A000000, 0x43924: 0x92803903, 0x43928: QUEUE_LO,
}


def verify_uart_opcodes(image: bytes) -> dict:
    for pc, expected in ROM_WORDS.items():
        if word(image, pc) != expected:
            raise ValueError(f'UART path ROM mismatch at {pc:#x}')
    return {'verified_rom_words': len(ROM_WORDS),
            'meaning': 'i960 opcode and immediate-operand anchors, not a live sound'}


def memory_event(e: dict) -> dict | None:
    if e.get('type') != 'memory' or e.get('kind') not in ('read', 'write'):
        return None
    a, n, raw = e.get('address'), e.get('size'), e.get('bytes')
    if not isinstance(a, int) or isinstance(a, bool) or not isinstance(n, int) or isinstance(n, bool):
        return None
    if n not in (1, 2, 4) or not isinstance(raw, str) or len(raw) != n * 2:
        return None
    try:
        value = int.from_bytes(bytes.fromhex(raw), 'little')
    except ValueError:
        return None
    if not (a < QUEUE_HI and a + n > QUEUE_LO or
            a < STAGE + 4 and a + n > STAGE or a == CURSOR or a == UART):
        return None
    return {'step': e.get('step'), 'kind': e['kind'], 'address': a,
            'size': n, 'value': value}


def observe_transport(lines, *, selected_writes=(), max_examples=64) -> dict:
    """Reject any missing writer/pop/stage/byte-read/UART/cursor/IRQ link.

    selected_writes are (exact step, physical ring slot, command) previously
    proved by the v0863 selector-to-filer witness on the SAME trace.
    """
    upstream = set(selected_writes)
    ring: dict[int, dict] = {}
    stats = Counter()
    witnessed: list[dict] = []
    pending: list[dict] = []
    active = None
    staged = None
    irq = False
    last_step = None

    def abandon(reason):
        nonlocal staged, active
        if staged or active:
            stats[reason] += 1
        staged = active = None

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
            m = memory_event(e)
            if m:
                pending.append(m)
            if len(pending) > 16:
                stats['memory_overflow'] += 1
                pending.clear()
                abandon('trace_incomplete')
                ring.clear()
                irq = False
            continue
        if e.get('type') != 'step':
            continue
        pc, after = e.get('ip_before'), e.get('ip_after')
        if not isinstance(pc, int) or isinstance(pc, bool) or not isinstance(after, int) or isinstance(after, bool):
            pending.clear()
            abandon('trace_incomplete')
            ring.clear()
            irq = False
            continue
        if last_step is not None and step != last_step + 1:
            stats['trace_step_gaps'] += 1
            ring.clear()
            abandon('trace_discontinuity')
            irq = False
        last_step = step
        mem = [m for m in pending if m['step'] == step]
        if len(mem) != len(pending):
            stats['memory_unpaired'] += len(pending) - len(mem)
            abandon('trace_incomplete')
            ring.clear()
        pending.clear()
        if pc == IRQ_CALL and after == DRAIN_ENTRY:
            if irq:
                abandon('nested_unclosed_interrupt')
            irq = True
            stats['vector_15_service_calls'] += 1
        for m in mem:
            a, n, value, kind = m['address'], m['size'], m['value'], m['kind']
            if kind == 'write' and a < QUEUE_HI and a + n > QUEUE_LO:
                # Any write touching a pending ring entry breaks its authorship,
                # including partial, boundary-crossing, and alien-PC stores.
                touched = [slot for slot in ring if a < slot + 4 and a + n > slot]
                for slot in touched:
                    del ring[slot]
                    stats['unconsumed_slot_overwritten'] += 1
                if pc == QUEUE_WRITE and QUEUE_LO <= a < QUEUE_HI and n == 4 and (a-QUEUE_LO)%4 == 0:
                    ring[a] = {'step': step, 'slot': a, 'command': value,
                               'selected': (step, a, value) in upstream}
                    stats['original_filer_ring_writes'] += 1
                else:
                    stats['unverified_ring_write'] += 1
            elif kind == 'read' and pc == POP_READ and irq and QUEUE_LO <= a < QUEUE_HI and n == 4 and (a-QUEUE_LO)%4 == 0:
                abandon('new_pop_before_previous_complete')
                writer = ring.pop(a, None)
                if writer is None or writer['command'] != value:
                    stats['unattributed_queue_pop'] += 1
                    active = None
                else:
                    active = {'writer': writer, 'command': value, 'pop_step': step}
                    stats['queue_pop_matched_to_original_filer'] += 1
            elif kind == 'write' and a < STAGE + 4 and a + n > STAGE:
                if pc == STAGE_WRITE and irq and a == STAGE and n == 4 and active and active['command'] == value:
                    staged = {**active, 'stage_step': step, 'bytes': [], 'pending_byte': None,
                              'cursor_pending': None, 'services': 0}
                    stats['stage_matches_queue_pop'] += 1
                else:
                    stats['unverified_stage'] += 1
                    abandon('unverified_stage_overwrite')
                active = None
            elif kind == 'read' and pc == BYTE_READ and irq and STAGE <= a < STAGE + 3 and n == 1:
                if staged and staged['pending_byte'] is None and staged['cursor_pending'] is None and len(staged['bytes']) < 3:
                    index = 2 - len(staged['bytes'])
                    expected = (staged['command'] >> (index*8)) & 255
                    if a == STAGE + index and value == expected:
                        staged['pending_byte'] = {'read_step': step, 'byte': expected}
                    else:
                        abandon('unexpected_stage_byte')
                else:
                    stats['unattributed_stage_byte_read'] += 1
            elif kind == 'write' and pc == UART_WRITE and irq and a == UART and n == 2:
                if staged and staged['pending_byte'] and step == staged['pending_byte']['read_step'] + 2 and value == staged['pending_byte']['byte']:
                    staged['cursor_pending'] = {'uart_step': step, 'byte': value}
                    staged['pending_byte'] = None
                    stats['verified_uart_byte'] += 1
                else:
                    stats['unattributed_uart_write'] += 1
                    abandon('uart_byte_mismatch_or_missing_read')
            elif kind == 'write' and a == CURSOR:
                if pc == CURSOR_WRITE and irq and n == 1 and staged and staged['cursor_pending'] and step == staged['cursor_pending']['uart_step'] + 2:
                    expected = (1, 0, 255)[len(staged['bytes'])]
                    if value == expected:
                        staged['bytes'].append(staged['cursor_pending']['byte'])
                        staged['cursor_pending'] = None
                        stats['verified_cursor_advance'] += 1
                    else:
                        abandon('unexpected_byte_cursor')
                else:
                    stats['unattributed_cursor_write'] += 1
                    abandon('unverified_cursor_write')
        if pc == DRAIN_RET and after == IRQ_RETURN:
            if not irq:
                stats['unpaired_isr_return'] += 1
            elif staged and (staged['pending_byte'] or staged['cursor_pending']):
                abandon('isr_returned_before_byte_complete')
            elif staged and len(staged['bytes']) == 3:
                record = {'producer_step': staged['writer']['step'],
                          'producer_pc': QUEUE_WRITE, 'ring_slot': staged['writer']['slot'],
                          'command': f"0x{staged['command']:08x}",
                          'uart_bytes': [f"0x{b:02x}" for b in staged['bytes']],
                          'selector_witness': staged['writer']['selected'],
                          'scope': 'i960 UART slot only; no 68000/SCSP claim'}
                if len(witnessed) < max_examples:
                    witnessed.append(record)
                stats['i960_ring_to_uart_complete'] += 1
                if record['selector_witness']:
                    stats['verified_selector_to_uart'] += 1
                staged = None
            irq = False
    if pending:
        stats['memory_unpaired'] += len(pending)
    if staged or active:
        abandon('trace_ended_before_delivery')
    return {'status': 'passive ROM-anchored i960 ring to UART witness',
            'counts': dict(sorted(stats.items())), 'witnesses': witnessed,
            'natural_gameplay_sound_proven': False,
            'limits': 'UART slot writes do not attest Motorola 68000 L3 reception, SCSP key-on or audibility'}


def upstream_writes(trace: Path, rom: Path) -> set[tuple[int, int, int]]:
    main = reconstruct_main_data_primary(rom)
    with trace.open(encoding='utf8', errors='replace') as source:
        selections = parse_observed_selections(source, main)['selections']
    with trace.open(encoding='utf8', errors='replace') as source:
        deliveries = witness_delivery(source, selections)['witnesses']
    pairs = set()
    for event in deliveries:
        if event['result'] != 'command_delivered_to_ring':
            continue
        for write in event['ring_writes']:
            if write['matches_selection']:
                pairs.add((write['step'], write['address'], int(write['value'], 16)))
    return pairs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path, help='local supported vf2.zip or ROM folder')
    parser.add_argument('trace', type=Path, help='genuine vf2probe --trace --memory-trace JSONL')
    parser.add_argument('--selector-chain', action='store_true',
                        help='also verify selector->dedup->queue via same local trace')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    rom = reconstruct_program(args.rom)
    proof = verify_uart_opcodes(rom)
    selected = set()
    if args.selector_chain:
        proof['upstream'] = verify_fighter_delivery_opcodes(rom)
        selected = upstream_writes(args.trace, args.rom)
    with args.trace.open(encoding='utf8', errors='replace') as source:
        result = observe_transport(source, selected_writes=selected)
    output = {'opcode_proof': proof, 'transport': result}
    if args.json:
        print(json.dumps(output, indent=2))
    else:
        print('Validated i960 opcode anchors:', proof)
        print('Transport counts:', result['counts'])
        for witness in result['witnesses']:
            print(witness)
        print(result['limits'])


if __name__ == '__main__':
    main()
