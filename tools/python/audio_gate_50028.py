#!/usr/bin/env python3
"""ROM-anchored i960 audio dispatch gate / producer provenance, read-only.

The scan is a static, aligned, direct-MEMB lower bound, not proof of
executable reachability. The optional vf2probe JSONL join is passive:
no ROM, snapshot, or trace contents are written by this program.
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path

from audio_field_xrefs import reconstruct_program

FLAG_ADDRESS = 0x00500028
DISPATCH_ENTRY = 0x00018A54
# From the ROM: control-flow sites are not inferred from data payloads.
GATE_EXPECTATIONS = {
    0x16150: (0x16158, 0x1615C, 0x16160),
    0x23070: (0x23078, 0x2308C, 0x2309C),
}
# Full absolute MEMB instruction forms accepted by this probe.
ABS_LOADS = {0x88: 'ldos', 0xC8: 'ldis', 0x90: 'ld'}
ABS_STORES = {0x8A: 'stos', 0xCA: 'stis', 0x92: 'st'}


def word_at(image: bytes, pc: int) -> int:
    if pc < 0 or pc % 4 or pc + 4 > len(image):
        raise ValueError(f'invalid aligned ROM address: {pc:#x}')
    return struct.unpack_from('<I', image, pc)[0]


def signed_displacement(encoded: int, mask: int, sign: int, size: int) -> int:
    displacement = encoded & mask
    return displacement - size if displacement & sign else displacement


def direct_call_target(image: bytes, pc: int) -> int | None:
    word = word_at(image, pc)
    if word >> 24 != 0x09:
        return None
    return (pc + signed_displacement(word, 0x00FFFFFC, 0x00800000,
                                      0x01000000)) & 0xFFFFFFFF


def absolute_flag_refs(image: bytes) -> list[dict]:
    """Only well-formed mode-0xC MEMB with 8-byte absolute operand.

    MEMA, MEMB relative, indexed and indirect aliases are not counted.
    """
    hits = []
    for pc in range(0, len(image) - 7, 4):
        word = word_at(image, pc)
        op = (word >> 24) & 255
        if op not in ABS_LOADS and op not in ABS_STORES:
            continue
        if ((word >> 10) & 15) != 12 or word_at(image, pc + 4) != FLAG_ADDRESS:
            continue
        hits.append({'pc': pc, 'opcode': (ABS_STORES | ABS_LOADS)[op],
                     'access': 'write' if op in ABS_STORES else 'read',
                     'reg': (word >> 19) & 31})
    return hits


def is_zero_ge_reg14_cobr(word: int) -> bool:
    """Decode the exact 'cmpibge 0, r14, target' instruction shape."""
    return (word >> 24 == 0x3B
            and (word >> 19) & 31 == 0
            and (word >> 14) & 31 == 14
            and (word >> 13) & 1 == 1
            and (word & 1) == 0)


def scan_gates(image: bytes) -> list[dict]:
    gates = []
    for load_pc, (branch_pc, call_pc, skip_pc) in GATE_EXPECTATIONS.items():
        if load_pc + 8 != branch_pc or branch_pc >= call_pc:
            raise ValueError('internal gate layout inconsistent')
        load = word_at(image, load_pc)
        if ((load >> 24) != 0xC8 or ((load >> 10) & 15) != 12
                or ((load >> 19) & 31) != 14
                or word_at(image, load_pc + 4) != FLAG_ADDRESS):
            raise ValueError(f'no verified signed short load at {load_pc:#x}')
        branch = word_at(image, branch_pc)
        if not is_zero_ge_reg14_cobr(branch):
            raise ValueError(f'gate opcode changed at {branch_pc:#x}')
        target = (branch_pc + signed_displacement(branch, 0x1FFC,
                                                  0x1000, 0x2000)) & 0xFFFFFFFF
        if target != skip_pc or direct_call_target(image, call_pc) != DISPATCH_ENTRY:
            raise ValueError(f'gate destination or direct call changed at {branch_pc:#x}')
        gates.append({'load_pc': load_pc, 'branch_pc': branch_pc,
                      'call_pc': call_pc, 'skip_pc': skip_pc,
                      'flag_address': FLAG_ADDRESS,
                      'call_condition': 'int16(flag) < 0',
                      'skip_condition': 'int16(flag) >= 0'})
    return gates


def as_i16(value: int) -> int:
    if not 0 <= value <= 0xFFFF:
        raise ValueError('not a 16-bit value')
    return value - 0x10000 if value & 0x8000 else value


def trace_gate(lines, *, limit: int = 64) -> dict:
    """Bounded event capture, associating memory and instruction by exact step.

    vf2probe logs memory events *before* step records. A trace step at a
    branch site reports the actual ip_after; no imaginary event is added
    when the branch does not execute. The resulting report is not audio.
    """
    branch_sites = {x[0] for x in GATE_EXPECTATIONS.values()}
    call_sites = {x[1] for x in GATE_EXPECTATIONS.values()}
    examples: list[dict] = []
    total = Counter()
    pending: list[dict] = []
    for line in lines:
        if not line.lstrip().startswith('{'):
            continue
        try:
            record = json.loads(line)
        except (TypeError, ValueError):
            continue
        if not isinstance(record, dict):
            continue
        step = record.get('step')
        if not isinstance(step, int) or isinstance(step, bool):
            continue
        if record.get('type') == 'memory' and record.get('address') == FLAG_ADDRESS:
            size, data = record.get('size'), record.get('bytes')
            if size != 2 or not isinstance(data, str) or len(data) != 4:
                continue
            try:
                value = int.from_bytes(bytes.fromhex(data), 'little')
            except ValueError:
                continue
            if record.get('kind') not in ('read', 'write'):
                continue
            pending.append({'step': step, 'kind': record['kind'],
                            'value': as_i16(value), 'producer_pc': None})
        if record.get('type') == 'step':
            ip, after = record.get('ip_before'), record.get('ip_after')
            if not isinstance(ip, int) or not isinstance(after, int):
                continue
            remaining = []
            for m in pending:
                if m['step'] == step:
                    m['producer_pc'] = ip
                    total['flag_' + m['kind']] += 1
                    if len(examples) < limit:
                        examples.append(m)
                elif m['step'] < step:
                    total['flag_without_matching_instruction'] += 1
                    if len(examples) < limit:
                        examples.append(m)
                else:
                    remaining.append(m)
            pending = remaining
            if ip in branch_sites:
                total['branch_executed'] += 1
                skip = next(x[2] for x in GATE_EXPECTATIONS.values() if x[0] == ip)
                if after == skip:
                    total['skipped_nonnegative'] += 1
                elif after == ip + 4:
                    total['fell_through_negative'] += 1
                else:
                    total['unclassified_branch'] += 1
                if len(examples) < limit:
                    examples.append({'step': step, 'kind': 'branch',
                                     'pc': ip, 'after': after, 'skip': skip})
            if ip in call_sites:
                total['dispatcher_call_executed'] += 1
                if len(examples) < limit:
                    examples.append({'step': step, 'kind': 'call',
                                     'pc': ip, 'after': after})
    for m in pending:
        total['flag_without_matching_instruction'] += 1
        if len(examples) < limit:
            examples.append(m)
    return {'status': 'passive trace analysis, NOT gameplay audio proof',
            'counts': dict(sorted(total.items())), 'examples': examples,
            'gameplay_sound_proven': False,
            'limits': 'No natural pre-contact replay; PC only from exact step; incomplete trace != no gameplay'}


def report(image: bytes) -> dict:
    refs = absolute_flag_refs(image)
    return {'status': 'static aligned ROM candidates, not execution',
            'flag_address': FLAG_ADDRESS,
            'reads': sum(x['access'] == 'read' for x in refs),
            'writes': sum(x['access'] == 'write' for x in refs),
            'write_sites': [f"0x{x['pc']:05x}" for x in refs if x['access'] == 'write'],
            'gates': scan_gates(image),
            'limitations': 'absolute direct MEMB only; not total field-writer coverage'}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom_zip', type=Path, help='user-local supported vf2.zip')
    p.add_argument('--trace', type=Path, help='optional local vf2probe --trace --memory-trace JSONL')
    p.add_argument('--json', action='store_true')
    args = p.parse_args()
    result = report(reconstruct_program(args.rom_zip))
    if args.trace is not None:
        with args.trace.open('r', encoding='utf-8', errors='replace') as stream:
            result['live_trace'] = trace_gate(stream)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print('Signed16 audio dispatch gate — ROM-aligned static evidence')
        print(f"0x{result['flag_address']:08x}: {result['reads']} direct reads, {result['writes']} direct writes")
        print('writers:', ', '.join(result['write_sites']))
        for gate in result['gates']:
            print(f"load={gate['load_pc']:#x} branch={gate['branch_pc']:#x} "
                  f"call={gate['call_pc']:#x} skip={gate['skip_pc']:#x}")
        if 'live_trace' in result:
            print('trace:', result['live_trace']['counts'])
        print(result['limitations'])


if __name__ == '__main__':
    main()
