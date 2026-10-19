#!/usr/bin/env python3
"""Opcode-verified, *static* A0 entry and descriptor-scan prediction.

Proves which branch the original 68000 code would take given a ROM
record and six explicitly supplied descriptor status bytes. No 68000
is executed; no mutable sound-board state or gameplay effect is claimed.
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from audio_a0_rows import (AUDIO_BASE, read_pieces, resolve_command,
                           InvalidRom)

# Exact original word-level decode; this is a strict VF2.1 ROM anchor.
# 0x1e98: lsl.w #2,d0; movea.l (a4,d0.w),a2;
# 0x1e9e: move.b (a2)+,d0; btst #7,d0; bne 0x1ed4.
# 0x1ed4: lea 0x2020(a6),a4; moveq #5,d7; btst #7,(a4);
#         beq 0x1eee; adda.l #0x20,a4; dbf d7,0x1eda;
#         jsr 0x1f04 (descriptor initializer).
WORDS = {
    0x1e98:0xe548,0x1e9a:0x2474,0x1e9c:0x0000,
    0x1e9e:0x101a,0x1ea0:0x0800,0x1ea2:0x0007,
    0x1ea4:0x6600,0x1ea6:0x002e,
    0x1ed4:0x49ee,0x1ed6:0x2020,0x1ed8:0x7e05,
    0x1eda:0x082c,0x1edc:0x0007,0x1ede:0x0000,
    0x1ee0:0x6700,0x1ee2:0x000c,0x1ee4:0xd9fc,
    0x1ee6:0x0000,0x1ee8:0x0020,0x1eea:0x51cf,
    0x1eec:0xffee,0x1eee:0x4eba,0x1ef0:0x0014,
    0x1f04:0x7e1f,
}


def check_opcodes(audio: bytes) -> dict:
    for pc, original in WORDS.items():
        if pc+2 > len(audio) or struct.unpack_from('>H',audio,pc)[0] != original:
            raise InvalidRom(f'A0 68000 opcode mismatch at {pc:#x}')
    # Branch displacement is relative to the extension-word PC.
    if (0x1ea6 + struct.unpack_from('>h', audio, 0x1ea6)[0] != 0x1ed4 or
        0x1ee2 + struct.unpack_from('>h', audio, 0x1ee2)[0] != 0x1eee or
        0x1eec + struct.unpack_from('>h', audio, 0x1eec)[0] != 0x1eda or
        0x1ef0 + struct.unpack_from('>h', audio, 0x1ef0)[0] != 0x1f04):
        raise InvalidRom('unexpected A0 branch / helper target')
    return {'verified_words':len(WORDS), 'index_entry':0x1e98,
            'set_bit7_branch':0x1ed4,'scan_base':0x3020,
            'scan_entries':6,'scan_stride':0x20,'helper_call':0x1f04}


def predict(audio: bytes, command: int, status_bytes: bytes) -> dict:
    proof = check_opcodes(audio)
    if len(status_bytes) != proof['scan_entries']:
        raise ValueError('must provide exactly 6 descriptor status bytes')
    row = resolve_command(audio, command)
    if row['record'] is None:
        raise InvalidRom('command exceeds the A0 directory max')
    first_byte = audio[row['record'] - AUDIO_BASE]
    if (first_byte & 0x80) == 0:
        return {'command':f'0x{command:08x}', 'record':row['record'],
                'first_byte_bit7':False, 'branch':0x1eae,
                'selected_slot':None, 'scope':'static prediction'}
    slot = next((i for i,b in enumerate(status_bytes) if not (b & 0x80)),None)
    return {'command':f'0x{command:08x}','record':row['record'],
            'first_byte_bit7':True,'branch':0x1ed4,
            'selected_slot':slot,
            'descriptor_address':0x3020+slot*0x20 if slot is not None else None,
            'scan_exhausted':slot is None,
            'scope':'static A0 branching only; not executed handler or keyed SCSP voice'}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path)
    p.add_argument('--command',type=lambda x:int(x,0),default=0x00ad2342)
    p.add_argument('--status',default='00,00,00,00,00,00',
                   help='six comma-separated hex descriptor status bytes')
    args=p.parse_args()
    _,audio=read_pieces(args.rom)
    try:
        statuses=bytes(int(x,16) for x in args.status.split(','))
    except ValueError as exc:
        p.error(f'invalid --status: {exc}')
    print(json.dumps({'opcode_proof':check_opcodes(audio),
                      'prediction':predict(audio,args.command,statuses)},indent=2))

if __name__ == '__main__':
    main()
