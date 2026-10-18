"""ROM-free tests with real vf2probe callback ordering (memory then step)."""
import json
import unittest
from audio_gate_causality import analyze, field_observation, overlap
from audio_gate_50028 import FLAG_ADDRESS


def e(**fields):
    return json.dumps(fields)


def mem(step, kind, addr, payload):
    return e(type='memory', step=step, kind=kind, address=addr,
             size=len(payload), bytes=payload.hex())


def s(step, before, after):
    return e(type='step', step=step, ip_before=before, ip_after=after)


class TestAudioGateCausality(unittest.TestCase):
    def test_32bit_store_low_half_and_one_byte_invalidation(self):
        self.assertEqual(field_observation(FLAG_ADDRESS, bytes.fromhex('ffff1122')),
                         (True, -1))
        self.assertEqual(field_observation(FLAG_ADDRESS, bytes.fromhex('ff')),
                         (True, None))
        self.assertEqual(field_observation(FLAG_ADDRESS - 1,
                                           bytes.fromhex('00ff7f')),
                         (True, 32767))

    def test_overlapping_write_is_not_ignored(self):
        self.assertTrue(overlap(FLAG_ADDRESS - 1, 2))
        self.assertTrue(overlap(FLAG_ADDRESS + 1, 2))
        self.assertFalse(overlap(FLAG_ADDRESS + 2, 4))

    def test_live_negative_gate_matched_to_writer(self):
        stream = [mem(1, 'write', FLAG_ADDRESS, bytes.fromhex('ffff0300')),
                  s(1, 0xbc3c, 0xbc44),
                  mem(2, 'read', FLAG_ADDRESS, bytes.fromhex('ffff')),
                  s(2, 0x23070, 0x23078),
                  s(3, 0x23078, 0x2307c),
                  s(4, 0x2307c, 0x23080),
                  s(5, 0x23080, 0x23084),
                  s(6, 0x23084, 0x23088),
                  s(7, 0x23088, 0x2308c),
                  s(8, 0x2308c, 0x18a54)]
        res = analyze(stream)
        self.assertEqual(res['counts']['calls_with_traced_gate'], 1)
        self.assertEqual(res['counts']['branch_value_matches'], 1)
        branch = next(r for r in res['examples'] if r['type'] == 'branch')
        self.assertEqual(branch['last_writer']['pc'], 0xbc3c)
        self.assertEqual(branch['loaded_value'], -1)
        self.assertFalse(res['natural_gameplay_audio_proven'])

    def test_skipped_gate(self):
        res = analyze([mem(10, 'read', FLAG_ADDRESS, bytes.fromhex('0000')),
                       s(10, 0x16150, 0x16158),
                       s(11, 0x16158, 0x16160)])
        self.assertEqual(res['counts']['branch_skip'], 1)
        self.assertEqual(res['counts']['branch_value_matches'], 1)
        self.assertNotIn('dispatcher_calls', res['counts'])

    def test_partial_write_invalidates_previous_certainty(self):
        res = analyze([mem(1, 'write', FLAG_ADDRESS, bytes.fromhex('0100')),
                       s(1, 0xe4bc, 0xe4c4),
                       mem(2, 'write', FLAG_ADDRESS+1, bytes.fromhex('80')),
                       s(2, 0xe4c4, 0x23070),
                       mem(3, 'read', FLAG_ADDRESS, bytes.fromhex('0180')),
                       s(3, 0x23070, 0x23078),
                       s(4, 0x23078, 0x2307c)])
        b = next(r for r in res['examples'] if r['type']=='branch')
        self.assertEqual(b['loaded_value'], -32767)
        self.assertFalse(b['last_writer']['complete'])

    def test_discontinuous_trace_does_not_join_stale_load(self):
        res = analyze([mem(8, 'read', FLAG_ADDRESS, bytes.fromhex('ffff')),
                       s(8, 0x23070, 0x23078),
                       s(12, 0x23078, 0x2307c)])
        branch=next(e for e in res['examples'] if e['type']=='branch')
        self.assertIsNone(branch['loaded_value'])
        self.assertEqual(res['counts']['trace_discontinuities'], 1)

    def test_mismatched_hardware_read_detected(self):
        res = analyze([mem(1,'read',FLAG_ADDRESS,bytes.fromhex('0000')),
                       s(1,0x23070,0x23078),s(2,0x23078,0x2307c)])
        self.assertEqual(res['counts']['branch_value_mismatch'],1)

    def test_trace_exits_gate_before_call(self):
        res = analyze([mem(1,'read',FLAG_ADDRESS,bytes.fromhex('ffff')),
                       s(1,0x23070,0x23078),s(2,0x23078,0x2307c),
                       s(3,0x2307c,0x23080),s(4,0x23080,0xdeadbeef),
                       s(5,0x2308c,0x18a54)])
        self.assertEqual(res['counts']['calls_without_traced_gate'],1)

    def test_malformed_json_and_unpaired_write(self):
        res = analyze(['broken','{}',mem(7,'write',FLAG_ADDRESS,bytes.fromhex('0100'))])
        self.assertEqual(res['counts']['unpaired_memory_events'],1)


if __name__ == '__main__':
    unittest.main()
