"""ROM-free fixtures plus optional user-local ROM-backed matrix regression."""
from __future__ import annotations

import os
import struct
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

from audio_command_matrix import (
    BUS_BASE, PTR_TABLE, PROFILE_COUNT, REGION_BYTES, ROM_BYTES,
    ROM_PAIR, COMMANDS_PER_PROFILE, InvalidSoundMatrix,
    decode_matrix, command_at, reconstruct_main_data_primary,
    reverse_lookup, summarize,
)


def w32(image: bytearray, address: int, value: int) -> None:
    struct.pack_into("<I", image, address - BUS_BASE, value)


def fixture() -> bytearray:
    region = bytearray(REGION_BYTES)
    base = BUS_BASE + 0x1e500
    for i in range(1, PROFILE_COUNT + 1):
        w32(region, PTR_TABLE + 4 * i, base + (i - 1) * COMMANDS_PER_PROFILE * 4)
    w32(region, base, 0x00ad2302)
    w32(region, base + 4, 0x00ad2405)
    w32(region, base + COMMANDS_PER_PROFILE * 4, 0x00ad2302)
    return region


class SoundCommandMatrixTests(unittest.TestCase):
    def test_sparse_profile_lookup(self):
        rows = decode_matrix(fixture())
        self.assertEqual((len(rows), len(rows[0])), (18, 11))
        self.assertEqual(command_at(rows, 1, 0), 0x00ad2302)
        self.assertEqual(command_at(rows, 1, 1), 0x00ad2405)
        self.assertIsNone(command_at(rows, 3, 0))

    def test_reverse_lookup_shared_command(self):
        rows = decode_matrix(fixture())
        self.assertEqual(reverse_lookup(rows, 0x00ad2302), [(1, 0), (2, 0)])
        self.assertEqual(reverse_lookup(rows, 0x00ad23ff), [])

    def test_summary_tracks_sparse_slots_without_inventing_events(self):
        result = summarize(decode_matrix(fixture()))
        self.assertEqual((result["nonzero_cells"], result["distinct_commands"],
                          result["commands_reused_across_cells"]), (3, 2, 1))
        self.assertEqual(result["empty_cells"], 195)
        self.assertIn("not proof", result["interpretation"])

    def test_invalid_profile_column(self):
        rows = decode_matrix(fixture())
        for profile, column in [(0, 0), (19, 0), (1, -1), (1, 11)]:
            with self.subTest(profile=profile, column=column):
                with self.assertRaises(IndexError):
                    command_at(rows, profile, column)

    def test_reject_external_and_unaligned_pointers(self):
        for pointer in [0, 0x01fffffc, 0x02000002, 0x02400000]:
            with self.subTest(pointer=pointer):
                region = fixture()
                w32(region, PTR_TABLE + 4, pointer)
                with self.assertRaises(InvalidSoundMatrix):
                    decode_matrix(region)

    def test_reject_wrong_command_family_and_nonzero_profile0(self):
        region = fixture()
        w32(region, BUS_BASE + 0x1e500, 0x003e4d22)
        with self.assertRaises(InvalidSoundMatrix):
            decode_matrix(region)
        region = fixture()
        w32(region, PTR_TABLE, BUS_BASE + 0x1e500)
        with self.assertRaises(InvalidSoundMatrix):
            decode_matrix(region)

    def test_truncated_rom(self):
        with self.assertRaises(InvalidSoundMatrix):
            decode_matrix(bytes(128))

    def test_zip_lane_interleave_local_fake_rom(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "test.zip"
            with ZipFile(source, "w") as archive:
                for i, name in enumerate(ROM_PAIR):
                    archive.writestr(name, bytes([10 + i, 20 + i]) * (ROM_BYTES // 2))
            data = reconstruct_main_data_primary(source)
            self.assertEqual(data[:8], bytes([10, 20, 11, 21] * 2))

    @unittest.skipUnless(os.environ.get("VF2_ROM_ZIP"), "set VF2_ROM_ZIP for ROM-backed regression")
    def test_user_rom_reference_census(self):
        source = Path(os.environ["VF2_ROM_ZIP"])
        rows = decode_matrix(reconstruct_main_data_primary(source))
        result = summarize(rows)
        self.assertEqual((result["nonzero_cells"], result["empty_cells"],
                          result["distinct_commands"],
                          result["commands_reused_across_cells"]), (107, 91, 98, 7))
        self.assertEqual(result["family_counts"], {"0x00ad23": 64, "0x00ad24": 43})

    def test_bad_size_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "test.zip"
            with ZipFile(source, "w") as archive:
                archive.writestr(ROM_PAIR[0], b"bad")
                archive.writestr(ROM_PAIR[1], b"bad")
            with self.assertRaises(InvalidSoundMatrix):
                reconstruct_main_data_primary(source)


if __name__ == "__main__":
    unittest.main()
