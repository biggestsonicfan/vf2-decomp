"""ROM-free tests for the cross-probe i960 sound evidence boundary."""
import unittest
from audio_natural_sound_gate import evaluate_transport


def witness(*, command='0x00ad2342', uart=None, selected=True, step=1, slot=0x504020):
    return {'producer_step': step, 'producer_pc': 0x43924,
            'ring_slot': slot, 'command': command,
            'uart_bytes': uart if uart is not None else ['0xad','0x23','0x42'],
            'selector_witness': selected}


def report(witnesses, selected=1, complete=1):
    return {'counts': {'i960_ring_to_uart_complete': complete,
                       'verified_selector_to_uart': selected},
            'witnesses': witnesses}


class TestNaturalSoundGate(unittest.TestCase):
    def test_verified_uart_still_not_audible_gameplay(self):
        result = evaluate_transport(report([witness()]))
        self.assertEqual(result['packets_sample'][0]['uart_packet'], 'AD2342')
        self.assertEqual(result['packets_sample'][0]['audio_driver_family'], 'AD')
        self.assertTrue(result['gates']['selector_to_i960_uart'])
        self.assertFalse(result['gates']['natural_fighter_event_proven'])
        self.assertFalse(result['gates']['scsp_keyon_from_live_gameplay_proven'])

    def test_unselected_transport_not_promoted(self):
        result = evaluate_transport(report([witness(selected=False)], 0))
        self.assertEqual(result['packets_sample'], [])
        self.assertEqual(result['status'], 'no_verified_selector_to_uart')

    def test_bad_uart_byte_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'disagrees'):
            evaluate_transport(report([witness(uart=['0xad', '0x23', '0x43'])]))

    def test_wrong_slot_or_pc_not_accepted(self):
        with self.assertRaisesRegex(ValueError, 'aligned'):
            evaluate_transport(report([witness(slot=0x504021)]))
        bad = witness(); bad['producer_pc'] = 0x43868
        with self.assertRaisesRegex(ValueError, 'original aligned'):
            evaluate_transport(report([bad]))

    def test_repeated_producer_rejected(self):
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            evaluate_transport(report([witness(), witness()], selected=2, complete=2))

    def test_count_mismatch_rejected(self):
        with self.assertRaisesRegex(ValueError, 'inconsistent'):
            evaluate_transport(report([], selected=2, complete=1))

    def test_truncated_ledger_keeps_total(self):
        result = evaluate_transport(report([witness()], 3, 3))
        self.assertEqual(result['verified_selector_to_uart_total'], 3)
        self.assertTrue(result['packets_truncated'])
        self.assertEqual(len(result['packets_sample']), 1)

    def test_zero_sample_limit_no_false_negative_count(self):
        result = evaluate_transport(report([witness()], 1, 1), max_packets=0)
        self.assertEqual(result['verified_selector_to_uart_total'], 1)
        self.assertEqual(result['packets_sample'], [])
        self.assertTrue(result['packets_truncated'])

    def test_no_witness_no_claim(self):
        result = evaluate_transport(report([], 0, 0))
        self.assertFalse(any(result['gates'].values()))

    def test_boolean_or_invalid_command_refused(self):
        bad = witness(); bad['producer_step'] = True
        with self.assertRaises(ValueError):
            evaluate_transport(report([bad]))
        with self.assertRaises(ValueError):
            evaluate_transport(report([witness(command='0x100000000')]))

    def test_other_command_family_not_overinterpreted(self):
        result = evaluate_transport(report([witness(command='0x00a01234',
                                                     uart=['0xa0','0x12','0x34'])]))
        self.assertEqual(result['packets_sample'][0]['audio_driver_family'], 'other')


if __name__ == '__main__':
    unittest.main()
