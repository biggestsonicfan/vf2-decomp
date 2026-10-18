"""ROM-free refuse/match tests, plus real local ROM static opcode test."""
from __future__ import annotations

import json
import os
import struct
import unittest
from pathlib import Path

from audio_selector_delivery import (
    verify_fighter_delivery_opcodes, witness_delivery, signed_target,
    LOAD_CMD, MOV_CMD, CALL_DEDUP, ENTRY_DEDUP, CALL_FILER, ENTRY_FILER,
    AFTER_FILER, DEDUP_RET, FILER_STORE, FILER_RET,
)
from audio_selector_provenance import ROUTES
from audio_field_xrefs import reconstruct_program
from audio_command_matrix import PTR_TABLE


def event(record_type, step, **kw):
    return json.dumps({'type': record_type, 'step': step, **kw})


def step(number, before, after):
    return event('step', number, ip_before=before, ip_after=after)


def write(number, value, addr=0x504020, size=4):
    return event('memory', number, kind='write', address=addr,
                 size=size, bytes=value.to_bytes(size, 'little').hex())


def selection(command=0xAD2307):
    return [{'route': 'fighter_driver', 'step': 3, 'command': command,
             'command_hex': f'0x{command:08x}'}]


def corridor(*, write_word=0xAD2307, with_write=True, return_filer=True,
             skip_mov=False, broken_call=False):
    lines = [step(3, LOAD_CMD, 0x18A9C),
             step(4, 0x18A9C, MOV_CMD)]
    if skip_mov:
        lines.append(step(5, MOV_CMD, CALL_FILER))
        lines.append(step(6, CALL_FILER, ENTRY_FILER))
        return lines
    lines.extend([
        step(5, MOV_CMD, CALL_DEDUP),
        step(6, CALL_DEDUP, ENTRY_DEDUP if not broken_call else 0x43000),
        step(7, ENTRY_DEDUP if not broken_call else 0x43000, DEDUP_RET),
        step(8, DEDUP_RET, CALL_FILER),
        step(9, CALL_FILER, ENTRY_FILER),
        step(10, ENTRY_FILER, FILER_STORE),
    ])
    if with_write:
        lines.append(write(11, write_word))
    lines.append(step(11, FILER_STORE, FILER_RET))
    if return_filer:
        lines.append(step(12, FILER_RET, AFTER_FILER))
    return lines


def fixture_rom():
    img = bytearray(0x43A00)
    for char, ptr, cmd, fighter_reg, row_reg in ROUTES.values():
        struct.pack_into('<I', img, char,
                         0x80002000 | (4 << 19) | (fighter_reg << 14) | 0x1B1)
        struct.pack_into('<I', img, ptr,
                         0x90003800 | (row_reg << 19) | 0x100 | 3)
        struct.pack_into('<I', img, ptr + 4, PTR_TABLE)
        struct.pack_into('<I', img, cmd,
                         0x90001C00 | (5 << 19) | (row_reg << 14) | 0x100 | 4)
    struct.pack_into('<I', img, MOV_CMD, 0x5C801605)
    for pc, target in ((CALL_DEDUP, ENTRY_DEDUP), (CALL_FILER, ENTRY_FILER)):
        struct.pack_into('<I', img, pc, 0x09000000 | ((target-pc)&0x00FFFFFC))
    struct.pack_into('<I', img, FILER_STORE, 0x92803903)
    struct.pack_into('<I', img, FILER_STORE + 4, 0x00504020)
    struct.pack_into('<I', img, DEDUP_RET, 0x0A000000)
    struct.pack_into('<I', img, FILER_RET, 0x0A000000)
    return img


class TestSelectorDelivery(unittest.TestCase):
    def test_static_copy_and_calls(self):
        proof = verify_fighter_delivery_opcodes(fixture_rom())
        self.assertEqual(proof['mov_selected_to_g0'], '0x18ab8')
        self.assertEqual(signed_target(CALL_DEDUP, 0x0902AEF0), ENTRY_DEDUP)

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'), 'user ROM only')
    def test_three_real_rom_paths_and_delivery_opcodes(self):
        image = reconstruct_program(Path(os.environ['VF2_ROM_ZIP']))
        proof = verify_fighter_delivery_opcodes(image)
        self.assertEqual(proof['queue_filer_call'], '0x18ac0')

    def test_wrong_rom_move_refused(self):
        image = fixture_rom()
        struct.pack_into('<I', image, MOV_CMD, 0x5C801604)
        with self.assertRaises(ValueError):
            verify_fighter_delivery_opcodes(image)

    def test_wrong_rom_call_refused(self):
        image = fixture_rom()
        struct.pack_into('<I', image, CALL_DEDUP, 0x09000000)
        with self.assertRaises(ValueError):
            verify_fighter_delivery_opcodes(image)

    def test_reject_write_at_embedded_extension_word(self):
        lines = corridor()
        lines[-2] = step(11, 0x4388C, FILER_RET)
        lines[7] = step(10, ENTRY_FILER, 0x4388C)
        result = witness_delivery(lines, selection())
        self.assertNotIn('command_delivered_to_ring', result['counts'])
        self.assertGreater(result['counts'].get('queue_write_at_unverified_opcode', 0), 0)

    def test_reject_fake_dedup_return(self):
        lines = corridor()
        lines[5] = step(8, 0x439B4, CALL_FILER)
        result = witness_delivery(lines, selection())
        self.assertNotIn('command_delivered_to_ring', result['counts'])

    def test_reject_fake_filer_return(self):
        lines = corridor()
        lines[-1] = step(12, 0x438F8, AFTER_FILER)
        result = witness_delivery(lines, selection())
        self.assertNotIn('command_delivered_to_ring', result['counts'])

    def test_reject_wrong_ring_store_opcode(self):
        image = fixture_rom()
        struct.pack_into('<I', image, FILER_STORE, 0x92803904)
        with self.assertRaises(ValueError):
            verify_fighter_delivery_opcodes(image)

    def test_reject_wrong_dedup_ret_opcode(self):
        image = fixture_rom()
        struct.pack_into('<I', image, DEDUP_RET, 0x08000000)
        with self.assertRaises(ValueError):
            verify_fighter_delivery_opcodes(image)

    def test_matched_word_inside_actual_call_return(self):
        report = witness_delivery(corridor(), selection())
        self.assertEqual(report['counts']['command_delivered_to_ring'], 1)
        self.assertTrue(report['witnesses'][0]['matching_complete_ring_write'])
        self.assertTrue(report['witnesses'][0]['dedup_entered'])
        self.assertTrue(report['witnesses'][0]['filer_entered'])
        self.assertFalse(report['natural_combat_audio_proven'])

    def test_different_word_not_matched(self):
        report = witness_delivery(corridor(write_word=0xAD2404), selection())
        self.assertEqual(report['counts']['filer_returned_without_matching_ring_word'], 1)
        self.assertFalse(report['witnesses'][0]['matching_complete_ring_write'])

    def test_skipping_mov_fails_closed(self):
        report = witness_delivery(corridor(skip_mov=True), selection())
        self.assertEqual(report['counts']['skipped_command_move'], 1)
        self.assertNotIn('command_delivered_to_ring', report['counts'])

    def test_broken_call_fails_closed(self):
        report = witness_delivery(corridor(broken_call=True), selection())
        self.assertIn('unexpected_after_mov', report['counts'])

    def test_partial_ring_write_fails_closed(self):
        lines = corridor(with_write=False)
        lines.insert(-2, write(11, 0x2307, size=2))
        report = witness_delivery(lines, selection())
        self.assertEqual(report['counts']['partial_ring_writes_inside_filer'], 1)
        self.assertEqual(report['counts']['filer_returned_without_matching_ring_word'], 1)

    def test_no_filer_return_not_delivered(self):
        report = witness_delivery(corridor(return_filer=False), selection())
        self.assertEqual(report['counts']['trace_ended_before_filer_return'], 1)
        self.assertNotIn('command_delivered_to_ring', report['counts'])

    def test_trace_gap_invalidates_active(self):
        lines = corridor()
        lines[7] = step(99, CALL_FILER, ENTRY_FILER)
        report = witness_delivery(lines, selection())
        self.assertGreater(report['counts']['trace_discontinuities'], 0)
        self.assertNotIn('command_delivered_to_ring', report['counts'])

    def test_preceding_unrelated_queue_entry_excluded(self):
        lines = [write(2, 0xAD2307), step(2, 0x438F8, LOAD_CMD)] + corridor()
        report = witness_delivery(lines, selection())
        self.assertEqual(report['counts']['command_delivered_to_ring'], 1)
        self.assertEqual(len(report['witnesses'][0]['ring_writes']), 1)

    def test_empty_selection_not_admitted(self):
        report = witness_delivery(corridor(write_word=0), selection(0))
        self.assertNotIn('verified_selections_seen', report['counts'])

    def test_malformed_incomplete_trace_does_not_admit(self):
        report = witness_delivery(['bad', '{}', '{'] + corridor()[:4], selection())
        self.assertEqual(report['counts']['trace_ended_before_filer_return'], 1)
        self.assertFalse(report['natural_combat_audio_proven'])


if __name__ == '__main__':
    unittest.main()
