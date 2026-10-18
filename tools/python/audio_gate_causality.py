#!/usr/bin/env python3
"""Passive, overlap-safe attribution of i960 0x500028 sound-dispatch gates.

Consumes genuine vf2probe --trace --memory-trace JSONL. Does not run the
emulator, infer event semantics or persist ROM/snapshot/trace contents.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from audio_gate_50028 import FLAG_ADDRESS, GATE_EXPECTATIONS, as_i16

FLAG_SIZE = 2
GATES = {branch: (load, call, skip) for load, (branch, call, skip)
         in GATE_EXPECTATIONS.items()}
LOADS = {load: branch for load, (branch, _, _) in GATE_EXPECTATIONS.items()}
CALLS = {call: branch for branch, (_, call, _) in GATES.items()}


def overlap(low: int, size: int) -> bool:
    return size > 0 and low < FLAG_ADDRESS + FLAG_SIZE and low + size > FLAG_ADDRESS


def field_observation(addr: int, raw: bytes) -> tuple[bool, int | None]:
    """Return (touched, full-signed16). Partial overlaps have unknown value.

    A 32-bit write at 0x500028 includes the low half, but an 8-bit
    write can only invalidate a previously known halfword value.
    """
    if not overlap(addr, len(raw)):
        return False, None
    start = FLAG_ADDRESS - addr
    if 0 <= start and start + FLAG_SIZE <= len(raw):
        return True, as_i16(int.from_bytes(raw[start:start + 2], 'little'))
    return True, None


def analyze(lines, *, limit: int = 128) -> dict:
    """Attribute memory accesses by EXACT step; retain uncertainty.

    Events are streamed in order, bounded independently of trace size.
    Invalid/partial events invalidate known writer state. A branch read
    is trusted only when read on the immediately previous instruction
    from its actual 8-byte load site. Mismatches are reported, not fixed.
    """
    counts: Counter[str] = Counter()
    samples: list[dict] = []
    pending: list[dict] = []
    writer: dict | None = None
    last_load: dict | None = None
    active_branch: dict | None = None
    last_step: int | None = None
    last_after: int | None = None

    def emit(event: dict) -> None:
        if len(samples) < limit:
            samples.append(event)

    for line in lines:
        if not line.lstrip().startswith('{'):
            continue
        try:
            event = json.loads(line)
        except (TypeError, ValueError):
            continue
        if not isinstance(event, dict):
            continue
        step = event.get('step')
        if not isinstance(step, int) or isinstance(step, bool) or step < 0:
            continue
        if event.get('type') == 'memory':
            addr, size, data = event.get('address'), event.get('size'), event.get('bytes')
            if not isinstance(addr, int) or not isinstance(size, int) or size > 16 or size < 1:
                continue
            if not overlap(addr, size):
                continue
            if event.get('kind') not in ('read', 'write'):
                continue
            full = isinstance(data, str) and len(data) == 2 * size
            try:
                raw = bytes.fromhex(data) if full else b''
            except (TypeError, ValueError):
                raw = b''
            _, value = field_observation(addr, raw) if len(raw) == size else (True, None)
            pending.append({'step': step, 'kind': event['kind'], 'addr': addr,
                            'width': size, 'value': value})
            continue
        if event.get('type') != 'step':
            continue
        ip, after = event.get('ip_before'), event.get('ip_after')
        if not isinstance(ip, int) or not isinstance(after, int):
            continue
        if last_step is not None and (step != last_step + 1 or last_after != ip):
            # Discontinuous log / IRQ entry / reset: never join a stale
            # signed load or call to a branch on a different path.
            active_branch = None
            last_load = None
            counts['trace_discontinuities'] += 1
        relevant = []
        future = []
        for m in pending:
            if m['step'] == step:
                relevant.append(m)
            elif m['step'] < step:
                counts['unpaired_memory_events'] += 1
                if m['kind'] == 'write':
                    writer = None
            else:
                future.append(m)
        pending = future
        # Preserve callback order for multiple reads/stores per opcode.
        for m in relevant:
            value = m['value']
            if m['kind'] == 'write':
                writer = {'step': step, 'pc': ip, 'signed_value': value,
                          'complete': value is not None}
                counts['field_write_complete' if value is not None else
                       'field_write_partial'] += 1
                emit({'type': 'field_write', **writer, 'address': m['addr'],
                      'width': m['width']})
            else:
                counts['field_read_complete' if value is not None else
                       'field_read_partial'] += 1
                if ip in LOADS:
                    last_load = {'step': step, 'pc': ip, 'signed_value': value,
                                 'complete': value is not None,
                                 'writer': dict(writer) if writer else None}
        if ip in GATES:
            load_pc, call_pc, skip = GATES[ip]
            observed = last_load if (last_load and last_load['pc'] == load_pc
                                     and last_load['step'] == step - 1) else None
            outcome = 'skip' if after == skip else ('fallthrough' if after == ip + 4
                                                     else 'unknown')
            if observed and observed['signed_value'] is not None and outcome != 'unknown':
                expected = 'skip' if observed['signed_value'] >= 0 else 'fallthrough'
                consistent = expected == outcome
                counts['branch_value_matches' if consistent else 'branch_value_mismatch'] += 1
            else:
                consistent = None
                counts['branch_value_unobserved'] += 1
            candidate = {'type': 'branch', 'step': step, 'pc': ip, 'outcome': outcome,
                         'loaded_value': observed['signed_value'] if observed else None,
                         'last_writer': observed['writer'] if observed else None,
                         'value_consistent': consistent, 'call_pc': call_pc}
            emit(candidate)
            counts['branch_' + outcome] += 1
            active_branch = dict(candidate) if outcome == 'fallthrough' else None
            last_load = None
        if ip in CALLS:
            counts['dispatcher_calls'] += 1
            associated = (active_branch if active_branch is not None and
                          active_branch['pc'] == CALLS[ip] and
                          active_branch['step'] < step <= active_branch['step'] + 16
                          else None)
            if associated:
                counts['calls_with_traced_gate'] += 1
            else:
                counts['calls_without_traced_gate'] += 1
            emit({'type': 'dispatch_call', 'step': step, 'pc': ip,
                  'target': after, 'associated_gate': associated})
            active_branch = None
        elif active_branch and ip not in GATES:
            # Only retain a candidate while the interpreter executes the
            # literal straight-line instructions between branch and call.
            if not (active_branch['pc'] < ip < active_branch['call_pc']):
                active_branch = None
        last_step = step
        last_after = after
    if pending:
        counts['unpaired_memory_events'] += len(pending)
    return {'status': 'passive causality candidates, NOT gameplay SFX proof',
            'counts': dict(sorted(counts.items())), 'examples': samples,
            'natural_gameplay_audio_proven': False,
            'limits': 'last observed writer may not be origin; incomplete traces cannot prove absence'}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    with args.trace.open('r', encoding='utf-8', errors='replace') as source:
        result = analyze(source)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(result['status'])
        print(result['counts'])
        for e in result['examples']:
            print(e)
        print(result['limits'])


if __name__ == '__main__':
    main()
