"""ROM-free evidence and failure-mode tests for optional 68000 L3 replay."""
import subprocess
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile
from types import SimpleNamespace

from audio_l3_packet_replay import replay_candidates, run_replay, unpack_owner_rom


def report(packet='AD2342', command='0x00ad2342', *, verified=True):
    return {
        'gates': {'selector_to_i960_uart': verified},
        'verified_selector_to_uart_total': 1,
        'packets_sample': [{'uart_packet': packet, 'command': command,
                            'producer_step': 12}]
    }


class TestL3PacketReplay(unittest.TestCase):
    def test_replay_exact_packet_and_marker(self):
        calls = []
        def runner(args, **kwargs):
            calls.append((args, kwargs))
            return SimpleNamespace(returncode=0, stdout='',
                                   stderr='MATCH C/68000 L3 (isolated packet fixture) 00ad2342: PASS\n')
        result = run_replay(report(), Path('bin'), Path('romdir'), runner=runner)
        self.assertTrue(result['all_replayed_match'])
        self.assertEqual(calls[0][0], ['bin', 'romdir', '--l3-packet', 'AD2342'])
        self.assertFalse(result['natural_fighter_event_proven'])
        self.assertFalse(result['live_scsp_keyon_proven'])

    def test_missing_original_selector_refused_before_running(self):
        def runner(*args, **kwargs):
            self.fail('cannot execute absent witness')
        with self.assertRaises(ValueError):
            run_replay(report(verified=False), Path('binary'), Path('romdir'), runner=runner)

    def test_no_packet_or_corrupt_count_refused(self):
        r = report()
        r['packets_sample'] = []
        with self.assertRaises(ValueError):
            replay_candidates(r)
        r = report()
        r['verified_selector_to_uart_total'] = 0
        with self.assertRaises(ValueError):
            replay_candidates(r)

    def test_wrong_command_or_high_byte_refused(self):
        with self.assertRaises(ValueError):
            replay_candidates(report(command='0x00ad2343'))
        with self.assertRaises(ValueError):
            replay_candidates(report(command='0x01ad2342'))

    def test_malformed_packet_and_boolean_step_refused(self):
        with self.assertRaises(ValueError):
            replay_candidates(report(packet='AD 342'))
        r = report()
        r['packets_sample'][0]['producer_step'] = True
        with self.assertRaises(ValueError):
            replay_candidates(r)

    def test_binary_success_without_marker_is_failure(self):
        fake = lambda *args, **kwargs: SimpleNamespace(returncode=0, stdout='', stderr='silent')
        r = run_replay(report(), Path('bin'), Path('romdir'), runner=fake)
        self.assertFalse(r['all_replayed_match'])

    def test_binary_failure_with_marker_is_failure(self):
        fake = lambda *args, **kwargs: SimpleNamespace(returncode=1, stdout='',
            stderr='MATCH C/68000 L3 (isolated packet fixture) 00ad2342:')
        self.assertFalse(run_replay(report(), Path('bin'), Path('romdir'),
                                    runner=fake)['all_replayed_match'])

    def test_timeout_fails_closed(self):
        def timeout(*args, **kwargs):
            raise subprocess.TimeoutExpired(cmd='bin', timeout=1)
        r = run_replay(report(), Path('bin'), Path('romdir'), runner=timeout)
        self.assertTrue(r['results'][0]['timeout'])
        self.assertFalse(r['all_replayed_match'])

    def test_missing_executable_fails_closed(self):
        def missing(*args, **kwargs):
            raise FileNotFoundError('bin')
        r = run_replay(report(), Path('bin'), Path('romdir'), runner=missing)
        self.assertEqual(r['results'][0]['execution_error'], 'FileNotFoundError')
        self.assertFalse(r['all_replayed_match'])

    def test_packet_limit(self):
        r = report()
        r['verified_selector_to_uart_total'] = 2
        r['packets_sample'].append({'uart_packet': 'AD2401', 'command': '0x00ad2401',
                                    'producer_step': 16})
        self.assertEqual(replay_candidates(r, limit=1), ['AD2342'])
        self.assertEqual(replay_candidates(r, limit=2), ['AD2342', 'AD2401'])
        with self.assertRaises(ValueError):
            replay_candidates(r, limit=0)

    def test_rom_archive_safe_extraction(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'test.zip'
            dest = Path(tmp) / 'rom'
            dest.mkdir()
            with ZipFile(source, 'w') as archive:
                archive.writestr('epr-17574.30', b'\x12\x34')
            unpack_owner_rom(source, dest)
            self.assertEqual((dest / 'epr-17574.30').read_bytes(), b'\x12\x34')

    def test_rom_archive_traversal_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'evil.zip'
            dest = Path(tmp) / 'rom'
            dest.mkdir()
            with ZipFile(source, 'w') as archive:
                archive.writestr('../escape', b'1234')
            with self.assertRaises(ValueError):
                unpack_owner_rom(source, dest)


if __name__ == '__main__':
    unittest.main()
