#!/usr/bin/env python3
"""Correlate verified i960 selector-to-UART events in one *local* VF2 trace.

This is a passive evidence gate, not a gameplay event injector or SCSP oracle.
It never reads/writes a snapshot, extracts samples or publishes trace contents.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def evaluate_transport(transport: dict, *, max_packets: int = 64) -> dict:
    """Reject unlinked packets; never promote a UART witness to audible SFX.

    The producer/selector/IRQ/POP/STAGE/UART chain has already been checked
    instruction-by-instruction by audio_uart_transport.observe_transport().
    Re-check record consistency here; a partial output list must be marked as
    truncated rather than treated as an exhaustive capture.
    """
    if max_packets < 0:
        raise ValueError('max_packets must be non-negative')
    counts = transport.get('counts')
    witnesses = transport.get('witnesses')
    if not isinstance(counts, dict) or not isinstance(witnesses, list):
        raise ValueError('missing transport counts or witnesses')
    n_complete = counts.get('i960_ring_to_uart_complete', 0)
    n_selected = counts.get('verified_selector_to_uart', 0)
    if (not isinstance(n_complete, int) or isinstance(n_complete, bool) or n_complete < 0
            or not isinstance(n_selected, int) or isinstance(n_selected, bool)
            or not 0 <= n_selected <= n_complete or len(witnesses) > n_complete):
        raise ValueError('inconsistent transport witness totals')
    packets = []
    seen = set()
    for item in witnesses:
        if not isinstance(item, dict):
            raise ValueError('invalid witness record')
        step, pc, slot = (item.get('producer_step'), item.get('producer_pc'),
                          item.get('ring_slot'))
        if (not isinstance(step, int) or isinstance(step, bool) or step < 0
                or pc != 0x43924 or not isinstance(slot, int) or isinstance(slot, bool)
                or not 0x504020 <= slot < 0x504060 or (slot - 0x504020) % 4):
            raise ValueError('witness lacks original aligned i960 producer')
        try:
            command = int(item.get('command', ''), 16)
            observed = [int(v, 16) for v in item.get('uart_bytes', [])]
        except (TypeError, ValueError) as exc:
            raise ValueError('non-hex command or UART bytes') from exc
        if not 0 <= command <= 0xFFFFFFFF or len(observed) != 3 or any(not 0 <= b <= 255 for b in observed):
            raise ValueError('out-of-range 3-byte UART packet')
        expected = [(command >> shift) & 255 for shift in (16, 8, 0)]
        if observed != expected:
            raise ValueError('UART payload disagrees with queued command')
        key = (step, slot, command)
        if key in seen:
            raise ValueError('duplicate producer identity in witness list')
        seen.add(key)
        if item.get('selector_witness') is not True:
            continue
        if len(packets) < max_packets:
            packets.append({
                'producer_step': step,
                'queue_slot': f'0x{slot:08x}',
                'command': f'0x{command:08x}',
                'uart_packet': ''.join(f'{byte:02X}' for byte in observed),
                # Hint from recovered command families only; not a claim of
                # a 68000 KEYON or that gameplay emitted this naturally.
                'audio_driver_family': 'AD' if observed[0] == 0xAD else 'other',
                'scope': 'verified i960 selector/queue/UART, not 68000 or SCSP',
            })
    # The upstream count may exceed the capped in-memory list. Check only
    # the directions that are independently observable in the sample.
    if len(packets) > n_selected:
        raise ValueError('verified packets exceed transport selector count')
    return {
        'status': ('i960_selector_to_uart_candidate_only' if n_selected
                   else 'no_verified_selector_to_uart'),
        'verified_transport_total': n_complete,
        'verified_selector_to_uart_total': n_selected,
        'packets_sample': packets,
        'packets_truncated': n_selected > len(packets),
        'gates': {
            'selector_to_i960_uart': n_selected > 0,
            'natural_fighter_event_proven': False,
            'm68k_l3_receive_proven': False,
            'm68k_c_differential_proven': False,
            'scsp_keyon_from_live_gameplay_proven': False,
            'audible_live_gameplay_proven': False,
        },
        'next_gate': ('capture a natural pre-contact selector event with frame progression'
                      if not n_selected else
                      'correlate this exact live packet with original 68000 L3 and SCSP state'),
        'limits': ('A strict trace proves i960 transport only; no inferred '
                   'fighter hit, hardware timing, 68000 reception or audio output.'),
    }


def analyze_rom_and_trace(rom: Path, trace: Path, *, max_packets: int = 64) -> dict:
    # Existing opcode, selector and transport oracles operate on the SAME
    # original ROM and immutable trace; no duplicated speculative semantics.
    from audio_field_xrefs import reconstruct_program
    from audio_uart_transport import (observe_transport, upstream_writes,
                                      verify_uart_opcodes)
    from audio_gate_causality import analyze as analyze_gate
    from audio_selector_delivery import verify_fighter_delivery_opcodes

    program = reconstruct_program(rom)
    proof = verify_uart_opcodes(program)
    upstream_opcode_proof = verify_fighter_delivery_opcodes(program)
    selected = upstream_writes(trace, rom)
    with trace.open(encoding='utf-8', errors='replace') as stream:
        transport = observe_transport(stream, selected_writes=selected,
                                      max_examples=max_packets)
    with trace.open(encoding='utf-8', errors='replace') as stream:
        gate = analyze_gate(stream)
    result = evaluate_transport(transport, max_packets=max_packets)
    result['original_rom_opcode_anchors'] = proof['verified_rom_words']
    result['selector_opcode_proof'] = upstream_opcode_proof
    result['independent_gate_observation_counts'] = gate['counts']
    result['transport_diagnostics'] = transport['counts']
    result['warning'] = ('Gate-0x50028 observation is separate evidence; it '
                         'is NOT causally joined to the selected UART packet.')
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path, help='user-owned original vf2.zip')
    parser.add_argument('trace', type=Path, help='local vf2probe --trace --memory-trace JSONL')
    parser.add_argument('--max-packets', type=int, default=64)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    result = analyze_rom_and_trace(args.rom, args.trace,
                                   max_packets=args.max_packets)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(result['status'])
        print('i960 selector->UART:', result['verified_selector_to_uart_total'])
        print('Packet samples:', result['packets_sample'])
        print('Next:', result['next_gate'])
        print(result['limits'])


if __name__ == '__main__':
    main()
