#!/usr/bin/env python3
"""ROM-free regression tests for sound i960 CTRL callsite census."""
import struct
import unittest
from audio_callsite_census import census, decode_direct_call


def call(pc, destination, op=0x09):
    off = (destination - pc) & 0x00FFFFFC
    return struct.pack("<I", (op << 24) | off)


class TestAudioCallsiteCensus(unittest.TestCase):
    def test_forward_and_backward_signed_relative(self):
        data = bytearray(0x50000)
        data[0x100:0x104] = call(0x100, 0x43888)
        data[0x43900:0x43904] = call(0x43900, 0x43888)
        self.assertEqual(decode_direct_call(data, 0x100), ("call", 0x43888))
        self.assertEqual(decode_direct_call(data, 0x43900), ("call", 0x43888))

    def test_bal_and_non_ctrl_refused(self):
        data = call(0, 0x43888, 0x0B) + struct.pack("<I", 0x902ADD44)
        self.assertEqual(decode_direct_call(data, 0), ("bal", 0x43888))
        self.assertIsNone(decode_direct_call(data, 4))
        self.assertIsNone(decode_direct_call(data, 3))
        self.assertIsNone(decode_direct_call(data, 8))

    def test_actual_adjacent_pair_and_indirect_refusal(self):
        data = bytearray(0x44000)
        data[0x43990:0x43994] = call(0x43990, 0x439AC)
        data[0x43994:0x43998] = call(0x43994, 0x43888)
        data[0x43800:0x43804] = struct.pack("<I", 0x86003000)
        res = census(data)
        self.assertEqual(res["callers"]["queue_filer"]["count"], 1)
        self.assertEqual(res["callers"]["command_dedup"]["count"], 1)
        self.assertEqual(res["callers"]["uart_drainer_fallthrough"]["count"], 0)
        self.assertEqual(res["adjacent_dedup_then_filer"],
                         [{"dedup_pc": "0x43990", "filer_pc": "0x43994"}])

    def test_second_dispatch_not_confused_with_filer(self):
        data = bytearray(0x24000)
        data[0x1615c:0x16160] = call(0x1615c, 0x18A54)
        data[0x2308c:0x23090] = call(0x2308c, 0x18A54)
        res = census(data)
        self.assertEqual(res["model2a_dispatch_two_direct_calls"],
                         ["0x1615c", "0x2308c"])
        self.assertEqual(res["callers"]["queue_filer"]["count"], 0)


if __name__ == "__main__":
    unittest.main()
