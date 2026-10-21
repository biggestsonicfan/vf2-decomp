"""ROM-free SOUND TEST tests plus opt-in original-ROM regression."""
from __future__ import annotations

import os
import struct
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

from audio_sound_test_catalog import (
    InvalidSoundTest, AKIRA_COMMAND, akira_streams,
    original_catalog, read_sound_program, records, reconstruct_program,
)


def fixture(*entries: tuple[int, str]) -> bytes:
    data = bytearray()
    for value, name in entries:
        data += struct.pack("<I", value) + name.encode("ascii") + b"\x00"
        while len(data) % 4:
            data.append(0)
    return bytes(data)


class SoundTestCatalog(unittest.TestCase):
    def test_variable_width_records(self):
        data = fixture((0x00AD1001, "Advertise"),
                       (0x00AD1004, "Akira"), (0x009E0A7F, "coin"))
        rows, end = records(data, start=0, count=3)
        self.assertEqual(end, len(data))
        self.assertEqual([x["name"] for x in rows],
                         ["Advertise", "Akira", "coin"])
        self.assertEqual([x["command"] for x in rows],
                         [0x00AD1001, AKIRA_COMMAND, 0x009E0A7F])
        self.assertEqual([x["address"] for x in rows], [0, 16, 28])

    def test_refuse_truncation(self):
        with self.assertRaises(InvalidSoundTest):
            records(fixture((AKIRA_COMMAND, "Akira")), start=0, count=2)

    def test_refuse_missing_null_terminator(self):
        with self.assertRaises(InvalidSoundTest):
            records(struct.pack("<I", AKIRA_COMMAND) + b"Akira", start=0,
                    count=1)

    def test_refuse_bad_padding(self):
        data = bytearray(fixture((AKIRA_COMMAND, "Akira")))
        data[-1] = 0x55
        with self.assertRaises(InvalidSoundTest):
            records(data, start=0, count=1)

    def test_refuse_non_ascii_label(self):
        data = bytearray(fixture((AKIRA_COMMAND, "Akira")))
        data[4] = 0x81
        with self.assertRaises(InvalidSoundTest):
            records(data, start=0, count=1)

    def test_refuse_unaddressable_word(self):
        with self.assertRaises(InvalidSoundTest):
            records(fixture((0, "Akira")), start=0, count=1)

    def test_refuse_false_original_catalog(self):
        with self.assertRaises(InvalidSoundTest):
            original_catalog(bytes(0x80000))

    def test_refuse_false_akira_directory(self):
        with self.assertRaises(InvalidSoundTest):
            akira_streams(bytes(0x80000))

    def test_refuse_tampered_program_rom(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            with ZipFile(p / "rom.zip", "w") as z:
                for chip in ("epr-18385.12", "epr-18386.13",
                             "epr-18387.14", "epr-18388.15"):
                    z.writestr(chip, b"\x00" * 0x20000)
            with self.assertRaises(InvalidSoundTest):
                reconstruct_program(p / "rom.zip")

    @unittest.skipUnless(os.environ.get("VF2_ROM_ZIP") or
                         os.environ.get("VF2_ROM_DIR"),
                         "original chips are opt-in, not public CI inputs")
    def test_original_sound_test_and_akira_streams(self):
        source = Path(os.environ.get("VF2_ROM_ZIP") or
                      os.environ["VF2_ROM_DIR"])
        catalog = original_catalog(reconstruct_program(source))
        info = akira_streams(read_sound_program(source))
        self.assertEqual((len(catalog), catalog[11]["address"],
                          catalog[11]["name"], catalog[11]["command"]),
                         (297, 0x76BE0, "Akira", 0x00AD1004))
        self.assertEqual((info["directory_row"], info["record"],
                          len(info["stream_pointers"])),
                         (0x608F24, 0x609C36, 6))


if __name__ == "__main__":
    unittest.main()
