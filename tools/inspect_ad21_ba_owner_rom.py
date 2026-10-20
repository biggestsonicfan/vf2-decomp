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
    # Original EPR reset vector and exact BA machine opcode shape.
    # This is an opcode-shape check, not a full M68000 execution.
    original_ssp = int.from_bytes(cpu[0:4], "big")
    original_entry = int.from_bytes(cpu[4:8], "big")
    assert original_ssp == 0x5000, "unexpected reset SSP"
    assert original_entry == 0x600100, "unexpected reset PC"
    assert cpu[0x1AC0:0x1AD6] == bytes.fromhex(
        "46fc2200 41f900605b26 3e18 43f85000 12d8 51cffffc"
    ), "BA prologue/staging DBF opcodes changed"
    assert cpu[0x1AD6:0x1AF0] == bytes.fromhex(
        "0240007f 0c00007f 67000066 43f900608df0"
        " 3219 b041 6e000058"
    ), "BA payload gate/max-selector branch changed"
    assert cpu[0x1B46:0x1B4C] == bytes.fromhex(
        "46fc2000 4e75"
    ), "BA early-return epilogue changed"

    # M68k Bcc.w displacement is relative to the word after opcode.
    def branch_target(pos):
        assert cpu[pos] in (0x67, 0x6E) and cpu[pos + 1] == 0
        disp = int.from_bytes(cpu[pos + 2:pos + 4], "big", signed=True)
        return pos + 2 + disp

    assert branch_target(0x1ADE) == 0x1B46, "BA 7f exit target changed"
    assert branch_target(0x1AEC) == 0x1B46, "BA row-max exit target changed"

    # Original ROM B0 dispatch: D2 = ring byte 1, D0 = ring byte 2,
    # the PC-relative 4-byte jump table is indexed by (D2 & 0x7f).
    # The command byte BA only establishes the B-class and channel A.
    assert cpu[0x13CC:0x13DC] == bytes.fromhex(
        "1400 1013 0242007f e54a 43fb2006 4ed1"
    ), "B0 dispatch selector opcodes changed"

    def b0_target(selector):
        slot = 0x13DE + selector * 4
        assert cpu[slot:slot + 2] == bytes.fromhex("4efa")
        disp = int.from_bytes(cpu[slot + 2:slot + 4],
                              "big", signed=True)
        return slot + 2 + disp

    b0_targets = {
        "0x2a": b0_target(0x2A),
        "0x07": b0_target(0x07),
        "0x3a": b0_target(0x3A),
    }
    assert b0_targets == {
        "0x2a": 0x194A, "0x07": 0x16BA, "0x3a": 0x1AC0
    }, "B0 jump-table destinations changed"

    # Boot descriptor image 1 row 0 is an active channel 0xA.
    # This admits B-class dispatch but does NOT select uploader 0x1ac0.
    image0 = int.from_bytes(cpu[0x3066:0x306A], "big")
    image1 = int.from_bytes(cpu[0x30A6:0x30AA], "big")
    assert (image0, image1) == (0x6030E6, 0x603186), "descriptor roots changed"
    descriptors = [
        cpu[(root - 0x600000) + index * 16:
            (root - 0x600000) + (index + 1) * 16]
        for root in (image0, image1) for index in range(10)
    ]
    channel_a_rows = [
        index for index, desc in enumerate(descriptors)
        if (desc[0] & 0x80) and desc[1] == 0x0A
    ]
    assert channel_a_rows == [10], "BA descriptor routing changed"
    assert 0x5000 <= 0x7F00 < 0x5000 + staging_bytes, (
        "old 0x7f00 test-stack hazard disappeared"
    )

    result = {
        "audio_rom_crc32": f"{AUDIO_CRC32:08x}",
        "sample_banks_verified": len(SAMPLE_CRCS),
        "cpu_rom_layout": "LOAD16_WORD_SWAP",
        "ad21_packets": packets,
        "uploader_3a_staging_bytes": staging_bytes,
        "uploader_3a_staging_source": "0x605b28",
        "uploader_3a_staging_target": "0x5000",
        "original_reset_ssp": f"0x{original_ssp:04x}",
        "original_reset_pc": f"0x{original_entry:06x}",
        "old_test_ssp_7f00_overwritten_by_staging": True,
        "ba_active_channel_a_descriptors": channel_a_rows,
        "uploader_3a_row_gate_exit_pc": "0x1b46",
        "uploader_3a_return_opcode": "4e75",
        "uploader_3a_row_max": row_max,
        "b0_dispatch_table_targets": {
            selector: f"0x{target:04x}"
            for selector, target in b0_targets.items()
        },
        "ad21_ba_packets_enter_uploader": False,
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
