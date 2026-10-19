#!/usr/bin/env python3
"""Recover the original 68000 L2 B-class stream-byte route for VF2 AD23/AD24.

Static ROM opcode + record-byte proof, NOT actual key-on or gameplay.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from audio_a0_rows import read_pieces, resolve_command, AUDIO_BASE, InvalidRom

# All executable words and operands needed for the first-byte-to-class jump.
WORDS = {
    0x1f92: 0x266a, 0x1f94: 0x0004,
    0x1f96: 0x08aa, 0x1f98: 0x0000, 0x1f9a: 0x0000,
    0x1f9c: 0x141b, 0x1f9e: 0x6b00, 0x1fa0: 0x0008,
    0x1fa8: 0x1542, 0x1faa: 0x0001,
    0x1fac: 0x1202, 0x1fae: 0x0281, 0x1fb0: 0x0000,
    0x1fb2: 0x00f0, 0x1fb4: 0xe409,
    0x1fb6: 0x41fb, 0x1fb8: 0x1004,
    0x1fba: 0x4ed0, 0x1fe8: 0x4efa, 0x1fea: 0x0012,
}


def opcode_proof(audio: bytes) -> None:
    for address, expected in WORDS.items():
        if address + 2 > len(audio) or struct.unpack_from('>H', audio, address)[0] != expected:
            raise InvalidRom(f'changed original 68000 instruction/operand at {address:#x}')
    branch = struct.unpack_from('>h', audio, 0x1fa0)[0]
    if 0x1fa0 + branch != 0x1fa8:
        raise InvalidRom('unexpected BMI path from 0x1f9e')


def route(audio: bytes, command: int) -> dict:
    opcode_proof(audio)
    selection = resolve_command(audio, command)
    if selection['record'] is None:
        raise InvalidRom('out-of-range selector')
    record = selection['record']
    start = record - AUDIO_BASE
    if start + 2 > len(audio):
        raise InvalidRom('truncated selected record')
    byte = audio[start + 1]  # A3 starts at record+1 after 0x1e9e MOVE.B (A2)+
    if byte & 0x80 == 0:
        raise InvalidRom('not the original negative stream-byte path')
    # MOVE.B (A3)+,D2, BMI; (D2&0xf0)>>2 is the 4-byte jump-table offset.
    offset = (byte & 0xf0) >> 2
    table_pc = 0x1fbc + offset
    if table_pc + 4 > len(audio) or struct.unpack_from('>H',audio,table_pc)[0] != 0x4efa:
        raise InvalidRom(f'unexpected L2 class table opcode at {table_pc:#x}')
    displacement = struct.unpack_from('>h', audio, table_pc+2)[0]
    target = table_pc+2+displacement
    if target >= len(audio) or target & 1:
        raise InvalidRom('invalid stream class handler target')
    return {'command':f'0x{command:08x}', 'record':f'0x{record:06x}',
            'stream_byte':f'0x{byte:02x}', 'negative_byte':True,
            'post_fetch_a3':f'0x{record+2:06x}',
            'class_table_pc':f'0x{table_pc:04x}',
            'handler':f'0x{target:04x}',
            'scope':'ROM static route only; not executed or played'}


def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path)
    p.add_argument('--command',type=lambda s:int(s,0),default=0x00ad2342)
    args=p.parse_args()
    _,audio=read_pieces(args.rom)
    print(json.dumps(route(audio,args.command),indent=2))

if __name__=='__main__':
    main()
