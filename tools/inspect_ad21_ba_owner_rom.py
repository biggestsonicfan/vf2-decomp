#!/usr/bin/env python3
"""Inspect authentic VF2.1 AD21 BA ROM tables without exporting ROM data.

This is STATIC inspection, not original-MC68000 execution/PCM parity.
Accepts an owner's ZIP or unpacked directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile
import zlib

AUDIO_NAME = "epr-17574.30"
AUDIO_SHA1 = "b6c0c3f0473bd7fc3ef4f5146110dfcc899a5af9"
AUDIO_CRC32 = 0x4D4C3A55
SAMPLE_CRCS = {
    "mpr-17573.31": 0xE43557FE,
    "mpr-17572.32": 0x4FEBECC8,
    "mpr-17571.36": 0x51CAA584,
    "mpr-17570.37": 0xBCCD324B,
}


def audit(path):
    if path.is_file():
        with ZipFile(path) as archive:
            members = {Path(name).name: name for name in archive.namelist()}
            raw = archive.read(members[AUDIO_NAME])
            samples = {name: archive.read(members[name]) for name in SAMPLE_CRCS}
    else:
        raw = (path / AUDIO_NAME).read_bytes()
        samples = {name: (path / name).read_bytes() for name in SAMPLE_CRCS}

    assert len(raw) == 0x80000, "unexpected audio ROM size"
    assert zlib.crc32(raw) == AUDIO_CRC32, "unexpected sound ROM CRC32"
    assert hashlib.sha1(raw).hexdigest() == AUDIO_SHA1, "unexpected sound ROM SHA1"
    for name, checksum in SAMPLE_CRCS.items():
        assert zlib.crc32(samples[name]) == checksum, f"sample bank mismatch: {name}"

    # MAME/project manifest: original EPR uses LOAD16_WORD_SWAP.
    cpu = bytearray(len(raw))
    cpu[0::2] = raw[1::2]
    cpu[1::2] = raw[0::2]
    word = lambda pos: int.from_bytes(cpu[pos:pos + 2], "big")
    staging_bytes = word(0x5B26) + 1
    row_max = word(0x8DF0)
    assert 0x5B28 + staging_bytes <= len(cpu), "staging beyond ROM"
    assert 0x5000 + staging_bytes <= 0x80000, "staging beyond sound RAM"
    assert cpu[0xA6B2] == 0xCF, "AD21 header changed"
    packets = []
    for index in range(12):
        pos = 0xA6B3 + index * 4
        packet = bytes(cpu[pos:pos + 4])
        expected = bytes((0xB9 + index % 6, 0x2A if index < 6 else 7,
                          0 if index < 6 else 0x7F, 0))
        assert packet == expected, f"AD21 packet {index} differs"
        packets.append(packet[:3].hex(" "))
    assert cpu[0xA6E3:0xA6E6] == b"\xff\x2f\x00", "AD21 terminator differs"
    result = {
        "audio_rom_crc32": f"{AUDIO_CRC32:08x}",
        "sample_banks_verified": len(SAMPLE_CRCS),
        "cpu_rom_layout": "LOAD16_WORD_SWAP",
        "ad21_packets": packets,
        "staging_copy_bytes": staging_bytes,
        "staging_source": "0x605b28",
        "staging_target": "0x5000",
        "ba_row_max": row_max,
        "ba_2a_row_admitted": 42 <= row_max,
        "ba_07_row_admitted": 7 <= row_max,
        "evidence": "static ROM data; no MC68000 interpreter run",
    }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="owner VF2 ROM ZIP or directory")
    args = parser.parse_args()
    print(json.dumps(audit(args.rom), indent=2))


if __name__ == "__main__":
    main()
