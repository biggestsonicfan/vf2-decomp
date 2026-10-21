#!/usr/bin/env python3
"""Audit VF2.1 SOUND TEST commands and Akira's music stream roots.

Static ROM inspection only: no music playback or gameplay-event claim.
Accepts a private VF2.1 ZIP or unpacked ROM directory. Never exports ROM data.
"""
from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path
from zipfile import ZipFile

PROGRAM_CHIPS = (
    ("epr-18385.12", 0, 0x78ED2D41),
    ("epr-18386.13", 2, 0x3418F428),
    ("epr-18387.14", 0x40000, 0x124A8453),
    ("epr-18388.15", 0x40002, 0x8D347980),
)
PROGRAM_BYTES = 0x80000
SOUND_CHIP = "epr-17574.30"
SOUND_CRC32 = 0x4D4C3A55
SOUND_TEST_TABLE = 0x76B48
SOUND_TEST_COUNT = 297
SOUND_TEST_END = 0x77A08
A0_DIRECTORY = 0x8E12
AUDIO_BASE = 0x600000
AKIRA_COMMAND = 0x00AD1004
AKIRA_RECORD = 0x609C36


class InvalidSoundTest(ValueError):
    """Unsupported VF2.1 ROM or unexpected SOUND TEST structure."""


def read_chip(source: Path, name: str) -> bytes:
    if source.is_dir():
        return (source / name).read_bytes()
    with ZipFile(source) as archive:
        names = {Path(info.filename).name: info.filename
                 for info in archive.infolist() if not info.is_dir()}
        return archive.read(names[name])


def reconstruct_program(source: Path) -> bytes:
    image = bytearray(PROGRAM_BYTES)
    for name, offset, crc in PROGRAM_CHIPS:
        raw = read_chip(source, name)
        if len(raw) != 0x20000 or zlib.crc32(raw) != crc:
            raise InvalidSoundTest(f"original main CPU ROM mismatch: {name}")
        image[offset:offset + len(raw) * 2:4] = raw[::2]
        image[offset + 1:offset + len(raw) * 2:4] = raw[1::2]
    return bytes(image)


def read_sound_program(source: Path) -> bytes:
    raw = read_chip(source, SOUND_CHIP)
    if len(raw) != 0x80000 or zlib.crc32(raw) != SOUND_CRC32:
        raise InvalidSoundTest("original audio CPU ROM mismatch")
    region = bytearray(len(raw))
    region[::2] = raw[1::2]
    region[1::2] = raw[::2]
    return bytes(region)


def records(image: bytes, start: int = SOUND_TEST_TABLE,
            count: int = SOUND_TEST_COUNT) -> tuple[list[dict], int]:
    """Each entry is a LE command, NUL-terminated label, then 4-byte padding."""
    pos = start
    result = []
    for index in range(count):
        if pos < 0 or pos + 5 > len(image):
            raise InvalidSoundTest(f"truncated SOUND TEST entry {index}")
        word = struct.unpack_from("<I", image, pos)[0]
        begin = pos + 4
        end = image.find(b"\x00", begin, min(begin + 48, len(image)))
        if end < 0 or end == begin:
            raise InvalidSoundTest(f"missing SOUND TEST label {index}")
        name_bytes = image[begin:end]
        if any(x < 0x20 or x > 0x7E for x in name_bytes):
            raise InvalidSoundTest(f"invalid SOUND TEST ASCII label {index}")
        after = (end + 4) & ~3
        if after > len(image) or any(image[end:after]):
            raise InvalidSoundTest(f"nonzero SOUND TEST padding {index}")
        if not 0 < word <= 0xFFFFFF:
            raise InvalidSoundTest(f"unexpected SOUND TEST command {index}")
        result.append({"index": index, "address": pos, "command": word,
                       "name": name_bytes.decode("ascii")})
        pos = after
    return result, pos


def original_catalog(program: bytes) -> list[dict]:
    rows, end = records(program)
    if end != SOUND_TEST_END or (
        rows[0]["name"], rows[0]["command"]
    ) != ("Advertise", 0x00AD1001) or (
        rows[1]["name"], rows[1]["command"]
    ) != ("stage clear", 0x00AD1006) or (
        rows[11]["name"], rows[11]["command"]
    ) != ("Akira", AKIRA_COMMAND) or (
        rows[34]["name"], rows[34]["command"]
    ) != ("coin", 0x009E0A7F) or (
        rows[296]["name"], rows[296]["command"]
    ) != ("Wo13", 0x00AD235A):
        raise InvalidSoundTest("VF2.1 SOUND TEST anchors changed")
    return rows


def akira_streams(audio: bytes) -> dict:
    def word(at: int) -> int:
        if at < 0 or at + 2 > len(audio):
            raise InvalidSoundTest("sound directory word out of bounds")
        return struct.unpack_from(">H", audio, at)[0]

    def long(at: int) -> int:
        if at < 0 or at + 4 > len(audio):
            raise InvalidSoundTest("sound directory long out of bounds")
        return struct.unpack_from(">I", audio, at)[0]

    row = long(A0_DIRECTORY + 0x10 * 4)
    if not AUDIO_BASE <= row < AUDIO_BASE + len(audio):
        raise InvalidSoundTest("AD10 index row not in sound ROM")
    maximum = word(row - AUDIO_BASE)
    record = long(row - AUDIO_BASE + 2 + 4 * 4)
    if maximum != 31 or record != AKIRA_RECORD:
        raise InvalidSoundTest("Akira AD10/04 directory lookup changed")
    pos = record - AUDIO_BASE
    if long(pos) != 0x10101010:
        raise InvalidSoundTest("Akira music record header changed")
    streams = []
    for i in range(32):
        pointer = long(pos + 4 + i * 4)
        if pointer == 0xFFFFFFF1:
            break
        if not AUDIO_BASE <= pointer < AUDIO_BASE + len(audio):
            raise InvalidSoundTest("Akira stream pointer invalid")
        streams.append(pointer)
    else:
        raise InvalidSoundTest("Akira music record has no terminator")
    if streams != [
        0x615773, 0x615AD7, 0x616009, 0x616B77, 0x6174AC, 0x617E0C
    ]:
        raise InvalidSoundTest("Akira stream roots changed")
    return {"directory_row": row, "directory_max": maximum,
            "record": record, "record_header": 0x10101010,
            "stream_pointers": streams}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--index", type=int, default=11,
                        help="SOUND TEST index (default: Akira, 11)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    catalog = original_catalog(reconstruct_program(args.rom))
    if not 0 <= args.index < len(catalog):
        parser.error("sound-test index must be 0..296")
    selected = catalog[args.index]
    info = {"sound_test_entries": len(catalog), "selection": {
        **selected, "command_hex": f"0x{selected['command']:08x}"},
        "scope": "static original ROM selection; NOT music playback"}
    if args.index == 11:
        info["akira_audio"] = akira_streams(read_sound_program(args.rom))
    if args.json:
        print(json.dumps(info, indent=2))
    else:
        print(f"SOUND TEST #{args.index}: {selected['name']}")
        print(f"Command: 0x{selected['command']:08x}")
        if "akira_audio" in info:
            row = info["akira_audio"]
            print(f"A0 directory: 0x{row['directory_row']:06x}")
            print(f"Sequence record: 0x{row['record']:06x}")
            print("Six stream roots: " + ", ".join(
                f"0x{p:06x}" for p in row["stream_pointers"]))
        print(info["scope"])


if __name__ == "__main__":
    main()
