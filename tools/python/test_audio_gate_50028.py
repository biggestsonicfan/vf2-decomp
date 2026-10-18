#!/usr/bin/env python3
"""ROM-free tests for strict i960 Model 2A sound dispatch gate analysis."""
from __future__ import annotations

import json
import struct
import unittest

from audio_gate_50028 import (
    FLAG_ADDRESS, GATE_EXPECTATIONS, absolute_flag_refs, as_i16,
    direct_call_target, is_zero_ge_reg14_cobr, report, scan_gates,
    trace_gate, word_at,
)


def make_image() -> bytearray:
    image = bytearray(0x24000)
    for load_pc, (branch_pc, call_pc, skip_pc) in GATE_EXPECTATIONS.items():
        struct.pack_into('<I', image, load_pc, 0xC8703000)
        struct.pack_into('<I', image, load_pc + 4, FLAG_ADDRESS)
        struct.pack_into('<I', image, branch_pc,
                         0x3B03A000 | ((skip_pc - branch_pc) & 0x1FFC))
        struct.pack_into('<I', image, call_pc,
                         0x09000000 | ((0x18A54 - call_pc) & 0x00FFFFFC))
    return image


def j(record):
    return json.dumps(record)


class TestAudioGate50028(unittest.TestCase):
    def test_two_rom_opcode_shapes(self):
        image = make_image()
        actual = scan_gates(image)
        self.assertEqual([(e['branch_pc'], e['call_pc'], e['skip_pc']) for e in actual],
                         [(0x16158, 0x1615C, 0x16160), (0x23078, 0x2308C, 0x2309C)])
        self.assertTrue(all(e['call_condition'] == 'int16(flag) < 0' for e in actual))

    def test_corrupted_branch_refused(self):
        image = make_image()
        struct.pack_into('<I', image, 0x23078, 0x3A03A024)
        with self.assertRaises(ValueError):
            scan_gates(image)

    def test_corrupted_target_refused(self):
        image = make_image()
        struct.pack_into('<I', image, 0x1615C, 0x090028F4)
        with self.assertRaises(ValueError):
            scan_gates(image)

    def test_abs_mode_only_and_correct_read_write(self):
        image = make_image()
        pc = 0x100
        struct.pack_into('<I', image, pc, 0xCA783000)
        struct.pack_into('<I', image, pc + 4, FLAG_ADDRESS)
        struct.pack_into('<I', image, pc+8, 0xC8783000)
        struct.pack_into('<I', image, pc+12, FLAG_ADDRESS)
        struct.pack_into('<I', image, pc+16, 0xCA781000)
        struct.pack_into('<I', image, pc+20, FLAG_ADDRESS)
        refs = absolute_flag_refs(image)
        self.assertEqual(len([x for x in refs if x['pc'] == 0x100]), 1)
        self.assertEqual(len([x for x in refs if x['pc'] == 0x108]), 1)
        self.assertEqual([x['access'] for x in refs if x['pc'] in (0x100, 0x108)],
                         ['write', 'read'])

    def test_i16_boundary_and_bad_address(self):
        self.assertEqual([as_i16(n) for n in (0, 0x7fff, 0x8000, 0xffff)],
                         [0, 32767, -32768, -1])
        with self.assertRaises(ValueError):
            as_i16(65536)
        with self.assertRaises(ValueError):
            word_at(bytes(128), 3)

    def test_negative_real_branch_and_call_trace(self):
        lines = [
            j({'type':'memory','step':10,'kind':'write',
               'address':FLAG_ADDRESS,'size':2,'bytes':'ffff'}),
            j({'type':'step','step':10,'ip_before':0xE4BC,'ip_after':0xE4C4}),
            j({'type':'memory','step':50,'kind':'read',
               'address':FLAG_ADDRESS,'size':2,'bytes':'ffff'}),
            j({'type':'step','step':50,'ip_before':0x23070,'ip_after':0x23078}),
            j({'type':'step','step':51,'ip_before':0x23078,'ip_after':0x2307C}),
            j({'type':'step','step':56,'ip_before':0x2308C,'ip_after':0x18A54}),
        ]
        result = trace_gate(lines)
        self.assertEqual(result['counts'], {
            'branch_executed': 1, 'dispatcher_call_executed': 1,
            'fell_through_negative': 1, 'flag_read': 1, 'flag_write': 1,
        })
        self.assertEqual(result['examples'][0]['producer_pc'], 0xE4BC)
        self.assertEqual(result['examples'][0]['value'], -1)
        self.assertFalse(result['gameplay_sound_proven'])

    def test_positive_skips_dispatch_and_incomplete_trace(self):
        lines = [
            j({'type':'memory','step':8,'kind':'write',
               'address':FLAG_ADDRESS,'size':2,'bytes':'0000'}),
            j({'type':'step','step':12,'ip_before':0x16158,'ip_after':0x16160}),
        ]
        result = trace_gate(lines)
        self.assertEqual(result['counts']['skipped_nonnegative'], 1)
        self.assertEqual(result['counts'].get('dispatcher_call_executed', 0), 0)
        self.assertEqual(result['counts']['flag_without_matching_instruction'], 1)

    def test_non_matching_memory_and_broken_json_refused(self):
        result = trace_gate(['oops', '{}', '{',
            j({'type':'memory','step':1,'kind':'write','address':FLAG_ADDRESS,
               'size':4,'bytes':'ffff0000'}),
            j({'type':'step','step':2,'ip_before':0x23078,'ip_after':0x777})])
        self.assertEqual(result['counts'], {'branch_executed': 1, 'unclassified_branch': 1})

    def test_direct_call_decoding(self):
        image = make_image()
        self.assertEqual(direct_call_target(image, 0x2308C), 0x18A54)
        self.assertTrue(is_zero_ge_reg14_cobr(word_at(image, 0x23078)))


if __name__ == '__main__':
    unittest.main()
