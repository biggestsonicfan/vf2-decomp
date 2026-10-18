"""ROM-free tests of indexed i960 opcode proof and trace selection attribution."""
from __future__ import annotations

import json
import os
from pathlib import Path
import struct
import unittest

from audio_command_matrix import BUS_BASE, REGION_BYTES, PTR_TABLE
from audio_selector_provenance import (ROUTES, assert_selector_opcodes,
    parse_observed_selections, attribute_queue_candidates)
from audio_field_xrefs import reconstruct_program


def w32(data, addr, value):
    struct.pack_into('<I', data, addr - BUS_BASE, value)


def fixture_rom():
    image = bytearray(0x1b100)
    for char_pc, ptr_pc, cmd_pc, reg, out in ROUTES.values():
        struct.pack_into('<I', image, char_pc,
                         0x80002000 | (4 << 19) | (reg << 14) | 0x1b1)
        struct.pack_into('<I', image, ptr_pc,
                         0x90003800 | (out << 19) | 0x100 | 3)
        struct.pack_into('<I', image, ptr_pc + 4, PTR_TABLE)
        struct.pack_into('<I', image, cmd_pc,
                         0x90001c00 | (5 << 19) | (out << 14) | 0x100 | 4)
    return image


def fixture_matrix():
    data = bytearray(REGION_BYTES)
    base = BUS_BASE + 0x1e600
    for profile in range(1, 19):
        ptr = base + (profile - 1) * 44
        w32(data, PTR_TABLE + profile * 4, ptr)
    w32(data, base + 4 * 4, 0x00ad2307)
    return data, base


def event(record_type, step, **kw):
    return json.dumps({'type': record_type, 'step': step, **kw})


def mem(step, addr, width, value):
    return event('memory', step, kind='read', address=addr,
                 size=width, bytes=value.to_bytes(width, 'little').hex())


def step(n, ip, after):
    return event('step', n, ip_before=ip, ip_after=after)


def selection_trace(data, ptr, *, column=4, value=0x00ad2307,
                    route='fighter_driver', extra=None):
    char, table, command, _base, _pointer = ROUTES[route]
    return [mem(1, 0x510b31, 1, column), step(1, char, table),
            mem(2, PTR_TABLE + 4, 4, ptr), step(2, table, command),
            *(extra or []),
            mem(3, ptr + column * 4, 4, value),
            step(3, command, command + 4)]


class TestSelectorProvenance(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("VF2_ROM_ZIP"), "VF2_ROM_ZIP required")
    def test_all_routes_recovered_from_real_user_rom(self):
        proof = assert_selector_opcodes(reconstruct_program(Path(os.environ["VF2_ROM_ZIP"])))
        self.assertEqual({r["route"] for r in proof}, set(ROUTES))

    def test_three_verified_selector_opcode_shapes(self):
        proof = assert_selector_opcodes(fixture_rom())
        self.assertEqual(len(proof), 3)
        for entry in proof:
            self.assertEqual((entry['row_index_register'],
                              entry['column_index_register'],
                              entry['command_result_register']), ('r3', 'r4', 'r5'))

    def test_incorrect_load_address_is_rejected(self):
        image = fixture_rom()
        struct.pack_into('<I', image, 0x10280, PTR_TABLE+4)
        with self.assertRaises(ValueError):
            assert_selector_opcodes(image)

    def test_incorrect_fighter_displacement_is_rejected(self):
        image = fixture_rom()
        word = struct.unpack_from('<I', image, 0x18a8c)[0]
        struct.pack_into('<I', image, 0x18a8c, (word & ~0xfff) | 0x1b0)
        with self.assertRaises(ValueError):
            assert_selector_opcodes(image)

    def test_incorrect_column_scale_is_rejected(self):
        image = fixture_rom()
        word = struct.unpack_from('<I', image, 0x1b07c)[0]
        struct.pack_into('<I', image, 0x1b07c, (word & ~0x380) | 0x80)
        with self.assertRaises(ValueError):
            assert_selector_opcodes(image)

    def test_selected_command_matches_local_rom(self):
        data, base = fixture_matrix()
        result = parse_observed_selections(selection_trace(data, base), data)
        self.assertEqual(result['counts']['rom_exact_selections'], 1)
        self.assertEqual((result['selections'][0]['row'],
                          result['selections'][0]['fighter_type'],
                          result['selections'][0]['command']),
                         (1, 4, 0x00ad2307))
        self.assertFalse(result['gameplay_audio_proven'])

    def test_wrong_command_is_not_admitted(self):
        data, base = fixture_matrix()
        result = parse_observed_selections(
            selection_trace(data, base, value=0x00ad2308), data)
        self.assertEqual(result['counts']['command_read_mismatch_or_incomplete'], 1)
        self.assertEqual(result['selections'], [])

    def test_pointer_value_different_from_rom_is_rejected(self):
        data, base = fixture_matrix()
        trace = selection_trace(data, base + 44)
        result = parse_observed_selections(trace, data)
        self.assertEqual(result['counts']['row_pointer_unverified'], 1)
        self.assertEqual(result['selections'], [])

    def test_outside_fighter_type_range_refuses(self):
        data, base = fixture_matrix()
        result = parse_observed_selections(selection_trace(data, base, column=11), data)
        self.assertEqual(result['counts']['character_index_unverified'], 1)
        self.assertEqual(result['selections'], [])

    def test_missing_middle_instruction_is_not_matched(self):
        data, base = fixture_matrix()
        original = selection_trace(data, base)
        result = parse_observed_selections(original[:2] + original[4:], data)
        self.assertEqual(result['selections'], [])

    def test_unobserved_pc_does_not_create_selection(self):
        data, base = fixture_matrix()
        original = selection_trace(data, base)
        result = parse_observed_selections(original[:1] + original[2:], data)
        self.assertEqual(result['selections'], [])

    def test_temporal_queue_match_not_causality(self):
        data, base = fixture_matrix()
        selections = parse_observed_selections(selection_trace(data, base), data)['selections']
        queue = [{'step': 4, 'kind': 'queue_entry', 'value': 0x00ad2307, 'pc': 0x438f4}]
        linked = attribute_queue_candidates(selections, queue)
        self.assertEqual(linked[0]['matching_queue_write_step'], 4)
        self.assertIn('not causal', linked[0]['queue_relationship'])

    def test_previous_or_too_late_queue_is_not_matched(self):
        data, base = fixture_matrix()
        selections = parse_observed_selections(selection_trace(data, base), data)['selections']
        queue = [{'step': 2, 'kind': 'queue_entry', 'value': 0x00ad2307, 'pc': 0x438f4},
                 {'step': 1000, 'kind': 'queue_entry', 'value': 0x00ad2307, 'pc': 0x438f4}]
        self.assertIsNone(attribute_queue_candidates(selections, queue)[0]['matching_queue_write_step'])

    def test_realistic_malformed_trace_does_not_claim_selection(self):
        data, _ = fixture_matrix()
        result = parse_observed_selections(['bad', '{', '{}'], data)
        self.assertEqual(result['selections'], [])


if __name__ == '__main__':
    unittest.main()
