#!/usr/bin/env python3
"""Passive VF2.1 original-ROM 90 HIGH routing for AD23/AD24 followups.

Proves only sound-ROM data admission, not a naturally executed 68000
instruction or SCSP key-on. No original record bytes are committed.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from audio_a0_rows import AUDIO_BASE, InvalidRom, read_pieces

COMMANDS = (0x9B4278, 0x9A017F)
BOOT_DESCRIPTOR_POINTERS = (0x603066, 0x6030A6)


def be32(rom: bytes, addr: int) -> int:
    off = addr - AUDIO_BASE
    if off < 0 or off+4 > len(rom) or addr & 1:
        raise InvalidRom(f'bad BE32 address {addr:#x}')
    return int.from_bytes(rom[off:off+4], 'big')


def get(rom: bytes, addr: int, size: int) -> bytes:
    off = addr-AUDIO_BASE
    if off < 0 or off+size > len(rom):
        raise InvalidRom(f'bad ROM access {addr:#x} size={size}')
    return rom[off:off+size]


def boot_descriptors(rom: bytes) -> list[bytes]:
    out = []
    for source in BOOT_DESCRIPTOR_POINTERS:
        image = be32(rom, source)
        for i in range(10):
            d = bytearray(get(rom, image+16*i, 16))
            scaled = (d[3] * 255) >> 8
            if scaled:
                d[10] = (scaled + 1) & 0xff
            out.append(bytes(d))
    return out


def route(rom: bytes, packet: int) -> dict:
    if packet not in COMMANDS:
        raise InvalidRom(f'not an identified AD-family followup {packet:#x}')
    command, payload, second = ((packet >> 16) & 255), ((packet >> 8) & 255), packet & 255
    matches = [(i,d) for i,d in enumerate(boot_descriptors(rom))
               if d[0] & 0x80 and d[1] == command & 0x0f]
    if len(matches) != 1:
        raise InvalidRom(f'expected one active channel descriptor, found {len(matches)}')
    idx, desc = matches[0]
    if second == 0:
        raise InvalidRom('zero second byte is a different 68000 path')
    mode = get(rom, AUDIO_BASE+0xc6a6+second, 1)[0]
    current = (payload + desc[4]) & 0xff
    if desc[2] < 0xf0:
        raise InvalidRom('boot descriptor not on HIGH 90 branch')
    select = (desc[2] - 0xf0) & 0x7f
    dirptr = be32(rom, AUDIO_BASE+0x32c6+4*select)
    entry = dirptr+6*(payload&0x7f)
    values = get(rom,entry,6)
    gate = int.from_bytes(values[:2], 'big')
    if gate == 0x00ff:
        raise InvalidRom('HIGH 90 table gate rejects packet')
    control = desc[6] if values[4] == 0x80 else values[4]
    release = desc[13] or (values[5] if desc[12] & 7 == 0 else desc[12])
    loop_b_byte = get(rom, AUDIO_BASE+0xc3e6+(current&0x7f), 1)[0]
    return {
        'packet':f'0x{packet:06x}', 'channel':command&0xf,
        'boot_descriptor':idx, 'boot_descriptor_mode':f'0x{desc[2]:02x}',
        'selected_mode':f'0x{mode:02x}',
        'lookup_current':f'0x{current:02x}',
        'row_base':f'0x{dirptr:06x}','table_entry':f'0x{entry:06x}',
        'lookup_word':f'0x{gate:04x}',
        'lookup_word_alt':f'0x{int.from_bytes(values[2:4],"big"):04x}',
        'lookup_control':f'0x{control:02x}',
        'lookup_release':f'0x{release:02x}',
        'loopb_lookup_byte':f'0x{loop_b_byte:02x}',
        'next_entry_pc':'0x0902',
        'scope':'ROM data admission; no 68000 trace or SCSP key-on verified'
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path)
    p.add_argument('--json',action='store_true')
    args=p.parse_args()
    _,audio=read_pieces(args.rom)
    results=[route(audio,cmd) for cmd in COMMANDS]
    if args.json:
        print(json.dumps(results,indent=2))
    else:
        for item in results: print(item)


if __name__ == '__main__': main()
