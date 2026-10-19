#!/usr/bin/env python3
"""Inspect ROM-anchored VF2.1 68000 A0 selection rows, with no ROM output.

Static pointer traversal; a resolved record does not mean an actual
execution reached it or that an SCSP voice was keyed.
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path
from zipfile import ZipFile

MAIN_BASE = 0x02000000
MAIN_SIZE = 0x400000
AUDIO_BASE = 0x600000
AUDIO_SIZE = 0x80000
SELECTOR_TABLE = 0x0201E44C
A0_ROWS = 0x608E12
PROFILES = 18
COLUMNS = 11
MAIN_PIECES = ('mpr-17560.10', 'mpr-17561.11')
AUDIO_PIECE = 'epr-17574.30'
FAMILIES = (0x00AD23, 0x00AD24)


class InvalidRom(ValueError):
    pass


def read_pieces(path: Path) -> tuple[bytes, bytes]:
    if path.is_dir():
        pieces = [(path / name).read_bytes() for name in MAIN_PIECES]
        sound = (path / AUDIO_PIECE).read_bytes()
    else:
        with ZipFile(path) as archive:
            pieces = [archive.read(name) for name in MAIN_PIECES]
            sound = archive.read(AUDIO_PIECE)
    if any(len(p) != 0x200000 for p in pieces) or len(sound) != AUDIO_SIZE:
        raise InvalidRom('unsupported VF2 ROM data lengths')
    main = bytearray(MAIN_SIZE)
    a, b = pieces
    main[::4], main[1::4] = a[::2], a[1::2]
    main[2::4], main[3::4] = b[::2], b[1::2]
    audio = bytearray(AUDIO_SIZE)
    audio[::2], audio[1::2] = sound[1::2], sound[::2]
    return bytes(main), bytes(audio)


def read32(data: bytes, address: int, base: int, endian: str) -> int:
    index = address - base
    if index < 0 or index & 1 or index + 4 > len(data):
        raise InvalidRom(f'invalid aligned ROM pointer {address:#x}')
    return struct.unpack_from(endian + 'I', data, index)[0]


def read16(data: bytes, address: int, base: int) -> int:
    index = address - base
    if index < 0 or index & 1 or index + 2 > len(data):
        raise InvalidRom(f'invalid 68000 word address {address:#x}')
    return struct.unpack_from('>H', data, index)[0]


def resolve_command(audio: bytes, command: int) -> dict:
    family = command >> 8
    if family not in FAMILIES or not 0 <= (command & 255) <= 255:
        raise InvalidRom(f'unsupported A0 command {command:#x}')
    row_index = family & 0x7f
    row_address = read32(audio, A0_ROWS + row_index * 4,
                         AUDIO_BASE, '>')
    maximum = read16(audio, row_address, AUDIO_BASE)
    selector = command & 255
    result = {'command': command, 'row': row_address, 'max': maximum,
              'selector': selector, 'in_bounds': selector <= maximum}
    if selector <= maximum:
        ptr = read32(audio, row_address + 2 + selector * 4,
                     AUDIO_BASE, '>')
        if ptr < AUDIO_BASE or ptr >= AUDIO_BASE + len(audio) or ptr & 1:
            raise InvalidRom(f'out-of-bank A0 record pointer {ptr:#x}')
        result['record'] = ptr
    else:
        result['record'] = None
    return result


def inspect(main: bytes, audio: bytes) -> dict:
    if read32(main, SELECTOR_TABLE, MAIN_BASE, '<') != 0:
        raise InvalidRom('expected null profile zero')
    entries = []
    for profile in range(1, PROFILES + 1):
        row = read32(main, SELECTOR_TABLE + 4 * profile,
                     MAIN_BASE, '<')
        if row < MAIN_BASE or row + COLUMNS * 4 > MAIN_BASE + len(main) or row & 3:
            raise InvalidRom(f'bad profile {profile} pointer {row:#x}')
        for column in range(COLUMNS):
            cmd = read32(main, row + column * 4, MAIN_BASE, '<')
            if cmd:
                result = resolve_command(audio, cmd)
                result.update({'profile': profile, 'column': column})
                entries.append(result)
    families = Counter(f'0x{e["command"] >> 8:06x}' for e in entries)
    with_record = [e for e in entries if e['record'] is not None]
    return {'total': len(entries), 'resolved': len(with_record),
            'out_of_max': len(entries) - len(with_record),
            'unique_commands': len({e['command'] for e in entries}),
            'unique_records': len({e['record'] for e in with_record}),
            'families': dict(sorted(families.items())),
            'entries': entries}


def verify_record_banks(audio: bytes) -> dict:
    """Prove two adjacent 128-row pointer sleds, with stride 12 bytes.

    Shape only: no guess about 12-byte record field semantics.
    """
    banks = []
    for family in FAMILIES:
        row_index = family & 0x7f
        row = read32(audio, A0_ROWS + row_index * 4, AUDIO_BASE, '>')
        maximum = read16(audio, row, AUDIO_BASE)
        if maximum != 127:
            raise InvalidRom(f'A0 family {family:#x} max changed: {maximum}')
        pointers = [read32(audio, row + 2 + 4 * i, AUDIO_BASE, '>')
                    for i in range(128)]
        if (pointers[0] < AUDIO_BASE or
                pointers[-1] + 12 > AUDIO_BASE + len(audio) or
                any(p & 1 for p in pointers) or
                any(pointers[i + 1] - pointers[i] != 12 for i in range(127))):
            raise InvalidRom(f'A0 family {family:#x} record strides changed')
        banks.append({'family': f'0x{family:06x}', 'row': row,
                      'first_record': pointers[0],
                      'end_exclusive': pointers[-1] + 12,
                      'records': len(pointers), 'stride': 12})
    if banks[0]['end_exclusive'] != banks[1]['first_record']:
        raise InvalidRom('A0 record banks are no longer adjacent')
    return {'record_count': 256, 'record_bytes': 12,
            'contiguous': True, 'banks': banks}


def main_cli() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom', type=Path)
    p.add_argument('--profile', type=int)
    p.add_argument('--column', type=int)
    p.add_argument('--json', action='store_true')
    args = p.parse_args()
    main, audio = read_pieces(args.rom)
    info = inspect(main, audio)
    info['record_banks'] = verify_record_banks(audio)
    data = {key: value for key, value in info.items() if key != 'entries'}
    if args.profile is not None or args.column is not None:
        if args.profile is None or args.column is None:
            p.error('--profile and --column must be supplied together')
        if not 1 <= args.profile <= PROFILES or not 0 <= args.column < COLUMNS:
            p.error('profile 1..18, column 0..10')
        result = next((e for e in info['entries']
                       if e['profile'] == args.profile and e['column'] == args.column), None)
        data['selection'] = ({key: f'0x{value:08x}' if key in
                              ('command', 'row', 'record') and value is not None else value
                              for key, value in result.items()}
                             if result else None)
    data['claim'] = 'static ROM record recovery only; not executed gameplay audio'
    if args.json:
        print(json.dumps(data, indent=2))
    else:
        for key, value in data.items():
            print(f'{key}: {value}')


if __name__ == '__main__':
    main_cli()
