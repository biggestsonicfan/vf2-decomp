#!/usr/bin/env python3
"""Opt-in, isolated C-vs-original-68000 L3 replay for witnessed i960 packets.

Only packets from a locally produced audio_natural_sound_gate report may
qualify as replay candidates. Neither a report nor an isolated replay proves
a naturally triggered gameplay hit, 68000 interrupt timing, or audible SCSP.
No sample, ROM or trace data is printed or uploaded by this utility.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path
from zipfile import ZipFile


def replay_candidates(report: dict, *, limit: int = 4) -> list[str]:
    if limit < 1:
        raise ValueError('limit must be >= 1')
    if not isinstance(report, dict) or not isinstance(report.get('gates'), dict):
        raise ValueError('not an audio natural sound gate report')
    if report['gates'].get('selector_to_i960_uart') is not True:
        raise ValueError('no verified i960 selector-to-UART event')
    count = report.get('verified_selector_to_uart_total')
    packets = report.get('packets_sample')
    if (not isinstance(count, int) or isinstance(count, bool) or count < 1
            or not isinstance(packets, list) or not packets or len(packets) > count):
        raise ValueError('no consistent selected packet sample to replay')
    selected = []
    for p in packets[:limit]:
        if not isinstance(p, dict):
            raise ValueError('invalid packet sample')
        packet = p.get('uart_packet')
        command = p.get('command')
        step = p.get('producer_step')
        if (not isinstance(packet, str) or re.fullmatch(r'[0-9a-fA-F]{6}', packet) is None
                or not isinstance(command, str) or re.fullmatch(r'0x[0-9a-fA-F]{8}', command) is None
                or not isinstance(step, int) or isinstance(step, bool) or step < 0):
            raise ValueError('malformed packet evidence')
        if int(command, 16) > 0xFFFFFF or int(packet, 16) != int(command, 16):
            raise ValueError('UART bytes disagree with 24-bit i960 command')
        selected.append(packet.upper())
    return selected


def run_replay(report: dict, executable: Path, rom_directory: Path, *,
               limit: int = 4, timeout: float = 30.0, runner=subprocess.run) -> dict:
    packets = replay_candidates(report, limit=limit)
    results = []
    for packet in packets:
        cmd = [str(executable), str(rom_directory), '--l3-packet', packet]
        try:
            proc = runner(cmd, capture_output=True, text=True, timeout=timeout)
            marker = f'MATCH C/68000 L3 (isolated packet fixture) {int(packet, 16):08x}:'
            # Both exit status AND distinctive receiver-only marker are required.
            matched = (proc.returncode == 0 and
                       marker in (proc.stdout or '') + (proc.stderr or ''))
            result = {'packet': packet, 'exit_code': proc.returncode,
                      'isolated_l3_differential_match': bool(matched)}
        except subprocess.TimeoutExpired:
            result = {'packet': packet, 'isolated_l3_differential_match': False,
                      'timeout': True}
        except OSError as exc:
            result = {'packet': packet, 'isolated_l3_differential_match': False,
                      'execution_error': type(exc).__name__}
        results.append(result)
    return {
        'status': 'isolated_68000_l3_fixture_replay_only',
        'replayed_count': len(results),
        'available_verified_uart_packets': report['verified_selector_to_uart_total'],
        'all_replayed_match': all(p['isolated_l3_differential_match'] for p in results),
        'results': results,
        'natural_fighter_event_proven': False,
        'actual_irq_timing_proven': False,
        'live_scsp_keyon_proven': False,
        'live_audible_output_proven': False,
        'limits': ('C-vs-stepper L3 result uses a fresh ROM-booted sound-board fixture '
                   'for each command, not synchronized game-session hardware state.'),
    }


def unpack_owner_rom(archive_path: Path, destination: Path) -> None:
    """Temporary extraction for the C ROM loader; reject traversal/bomb candidates."""
    with ZipFile(archive_path) as archive:
        files = [info for info in archive.infolist() if not info.is_dir()]
        if not files or len(files) > 80 or sum(f.file_size for f in files) > 64 * 1024 * 1024:
            raise ValueError('unsupported ROM archive size')
        names = [f.filename for f in files]
        if (len(names) != len(set(names)) or any(
                name.startswith('.') or '/' in name or '\\' in name or
                Path(name).name != name or info.file_size > 8 * 1024 * 1024
                for name, info in zip(names, files))):
            raise ValueError('unsafe ROM archive entry')
        for info in files:
            archive.extract(info, destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gate_report', type=Path,
                        help='private JSON from audio_natural_sound_gate.py --json')
    parser.add_argument('binary', type=Path, help='compiled vf2_m68k_l3_selection_tests')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--rom-directory', type=Path)
    group.add_argument('--rom-zip', type=Path)
    parser.add_argument('--limit', type=int, default=4)
    parser.add_argument('--timeout', type=float, default=30.0)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    with args.gate_report.open(encoding='utf-8') as f:
        report = json.load(f)
    # Fail BEFORE extracting ROMs or executing any binary on a missing witness.
    replay_candidates(report, limit=args.limit)
    if args.rom_zip is not None:
        with tempfile.TemporaryDirectory(prefix='vf2_l3_replay_') as path:
            unpack_owner_rom(args.rom_zip, Path(path))
            result = run_replay(report, args.binary, Path(path),
                                limit=args.limit, timeout=args.timeout)
    else:
        result = run_replay(report, args.binary, args.rom_directory,
                            limit=args.limit, timeout=args.timeout)
    print(json.dumps(result, indent=2))
    if not result['all_replayed_match']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
