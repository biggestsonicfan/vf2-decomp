#!/usr/bin/env python3
"""ROM-free regression tests for audio_field_xrefs."""
from __future__ import annotations
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path
from audio_field_xrefs import PROGRAM_LANES, decode_mema, reconstruct_program, report, scan


def mema(op: int, reg: int, base: int, displacement: int) -> bytes:
    return struct.pack("<I", ((op & 255) << 24) | (reg << 19) |
                       (base << 14) | (1 << 13) | displacement)


class TestAudioFieldXrefs(unittest.TestCase):
    def test_mema_decoder_refuses_unknown_forms(self) -> None:
        w = struct.unpack("<I", mema(0x82, 3, 23, 0x1B1))[0]
        self.assertEqual(decode_mema(w, {0x1B1}),
                         ("stob", "write", 1, 23, 3, 0x1B1))
        self.assertIsNone(decode_mema(w | (1 << 12), {0x1B1}))
        self.assertIsNone(decode_mema(w & ~(1 << 13), {0x1B1}))
        self.assertIsNone(decode_mema(w, {0x624}))

    def test_copy_and_noncopy(self) -> None:
        image = (mema(0xC0, 15, 23, 0x1B0)
                 + mema(0xC2, 15, 23, 0x1B1)
                 + mema(0x82, 3, 23, 0x1B1)
                 + mema(0x88, 14, 23, 0x624)
                 + mema(0x8A, 3, 23, 0x624))
        sites = scan(image)
        self.assertEqual([s.pc for s in sites], [4, 8, 12, 16])
        self.assertTrue(sites[0].preceding_copy)
        self.assertFalse(sites[1].preceding_copy)
        self.assertEqual(report(sites)["fields"][0]["candidate_count"], 2)

    def test_zero_writers_require_exact_adjacent_proof(self) -> None:
        zero_r15 = struct.pack("<I", 0x5C781E00)
        sites = scan(zero_r15 + mema(0xCA, 15, 23, 0x624)
                     + mema(0x8A, 15, 23, 0x624))
        self.assertEqual(len(sites), 2)
        self.assertTrue(sites[0].immediate_zero)
        self.assertFalse(sites[1].immediate_zero)
        self.assertEqual(report(sites)["fields"][1]["adjacent_zeroed_short_writer_count"], 1)
        clear = (struct.pack("<I", 0x5C181E00)
                 + mema(0x82, 3, 23, 0x2A)
                 + mema(0x8A, 3, 23, 0x624))
        self.assertTrue(scan(clear)[0].immediate_zero)

    def test_program_interleave(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            zip_path = Path(temp) / "vf2.zip"
            with zipfile.ZipFile(zip_path, "w") as z:
                for i, (name, _) in enumerate(PROGRAM_LANES):
                    z.writestr(name, bytes([0x10 + i, 0x20 + i]) * 0x10000)
            program = reconstruct_program(zip_path)
            self.assertEqual(program[0:4], bytes([0x10, 0x20, 0x11, 0x21]))
            self.assertEqual(program[0x40000:0x40004], bytes([0x12, 0x22, 0x13, 0x23]))
            self.assertEqual(len(program), 0x80000)

    def test_bounded_zero_store_run(self) -> None:
        from audio_field_xrefs import zero_via_store_run
        pack = lambda w: struct.pack("<I", w)
        clean = (pack(0x5C181E00) + pack(0x921DE198)
                 + pack(0x8A1DE614) + pack(0x8A1DE624))
        self.assertTrue(zero_via_store_run(clean, 12, 3, 23))
        self.assertFalse(zero_via_store_run(clean, 12, 4, 23))
        branch = (pack(0x5C181E00) + pack(0x09000004)
                  + pack(0x8A1DE614) + pack(0x8A1DE624))
        self.assertFalse(zero_via_store_run(branch, 12, 3, 23))

    def test_bad_rom_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            zip_path = Path(temp) / "bad.zip"
            with zipfile.ZipFile(zip_path, "w") as z:
                z.writestr(PROGRAM_LANES[0][0], b"incorrect")
            with self.assertRaises(ValueError):
                reconstruct_program(zip_path)


if __name__ == "__main__":
    unittest.main()
