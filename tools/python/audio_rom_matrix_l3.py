#!/usr/bin/env python3
"""Run the opt-in 98-command ROM-backed C/68000 L3 matrix from owner's ZIP.

Strictly an isolated receiver differential. This is NOT proof of natural
gameplay SFX, IRQ timing, actual KEYON or real hardware sound fidelity.
Never uploads or stores the owner's ROM; ZIP extraction is temporary.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

from audio_l3_packet_replay import unpack_owner_rom

MATRIX_SUMMARY = re.compile(
    r'^ROM sound-matrix L3: ([0-9]+) occupied, ([0-9]+) unique, ([0-9]+) passed$',
    re.MULTILINE,
)


def parse_result(exit_code: int, output: str) -> dict:
    matches = MATRIX_SUMMARY.findall(output)
    counts = tuple(int(v) for v in matches[0]) if len(matches) == 1 else None
    passed = exit_code == 0 and counts == (107, 98, 98)
    return {
        'scope': 'ROM-derived isolated L3 C-vs-original-68000 matrix only',
        'rom_matrix_verified': bool(passed),
        'occupied_cells': counts[0] if counts else None,
        'distinct_commands': counts[1] if counts else None,
        'passed_l3_differentials': counts[2] if counts else None,
        'expected': {'occupied_cells': 107, 'distinct_commands': 98},
        'natural_gameplay_event_proven': False,
        'real_irq_timing_proven': False,
        'live_scsp_keyon_proven': False,
        'audible_gameplay_proven': False,
        'limits': 'Isolated ROM-booted L3 fixtures; no contemporaneous game session.',
    }


def execute_matrix(executable: Path, rom_directory: Path, *,
                   timeout: float = 600.0, runner=subprocess.run) -> dict:
    if timeout <= 0:
        raise ValueError('timeout must be positive')
    command = [str(executable), str(rom_directory), '--matrix-l3']
    try:
        proc = runner(command, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {**parse_result(1, ''), 'error': 'timeout'}
    except OSError as exc:
        return {**parse_result(1, ''), 'error': type(exc).__name__}
    result = parse_result(proc.returncode, (proc.stdout or '') + '\n' + (proc.stderr or ''))
    result['exit_code'] = proc.returncode
    return result


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('binary', type=Path,
                   help='compiled vf2_m68k_l3_selection_tests executable')
    sources = p.add_mutually_exclusive_group(required=True)
    sources.add_argument('--rom-zip', type=Path, help='local original VF2.1 ROM ZIP')
    sources.add_argument('--rom-directory', type=Path, help='original VF2.1 ROM directory')
    p.add_argument('--timeout', type=float, default=600.0)
    args = p.parse_args()
    if args.timeout <= 0:
        p.error('--timeout must be positive')
    if args.rom_zip is not None:
        with tempfile.TemporaryDirectory(prefix='vf2_l3_matrix_') as directory:
            unpack_owner_rom(args.rom_zip, Path(directory))
            result = execute_matrix(args.binary, Path(directory), timeout=args.timeout)
    else:
        result = execute_matrix(args.binary, args.rom_directory, timeout=args.timeout)
    print(json.dumps(result, indent=2))
    if not result['rom_matrix_verified']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
