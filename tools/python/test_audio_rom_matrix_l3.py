"""ROM-free matrix runner: do not report an unrun matrix as validated."""
import subprocess
import unittest
from pathlib import Path
from types import SimpleNamespace

from audio_rom_matrix_l3 import execute_matrix, parse_result


class TestAudioRomMatrixL3(unittest.TestCase):
    def test_strict_success_needs_exit_and_all_98(self):
        output='ROM sound-matrix L3: 107 occupied, 98 unique, 98 passed\n'
        r=parse_result(0, output)
        self.assertTrue(r['rom_matrix_verified'])
        self.assertFalse(r['audible_gameplay_proven'])
        self.assertFalse(parse_result(1,output)['rom_matrix_verified'])
        self.assertFalse(parse_result(0,'ROM sound-matrix L3: 107 occupied, 98 unique, 97 passed')['rom_matrix_verified'])

    def test_no_summary_or_ambiguous_summary_refused(self):
        self.assertFalse(parse_result(0, '')['rom_matrix_verified'])
        output='ROM sound-matrix L3: 107 occupied, 98 unique, 98 passed\n'
        self.assertFalse(parse_result(0, output+output)['rom_matrix_verified'])

    def test_fixture_runner_invocation(self):
        calls=[]
        def runner(argv, **kwargs):
            calls.append((argv,kwargs))
            return SimpleNamespace(returncode=0,stdout='ROM sound-matrix L3: 107 occupied, 98 unique, 98 passed\n',stderr='')
        r=execute_matrix(Path('vf2_m68k_l3_selection_tests'),Path('roms/vf2'),runner=runner)
        self.assertTrue(r['rom_matrix_verified'])
        self.assertEqual(calls[0][0],
                         [str(Path('vf2_m68k_l3_selection_tests')),
                          str(Path('roms') / 'vf2'), '--matrix-l3'])
        self.assertEqual(calls[0][1]['timeout'],600.0)

    def test_timeout_refused(self):
        def runner(*args,**kwargs):
            raise subprocess.TimeoutExpired('test',1)
        r=execute_matrix(Path('binary'),Path('rom'),runner=runner)
        self.assertEqual(r['error'],'timeout')
        self.assertFalse(r['rom_matrix_verified'])

    def test_missing_binary_refused(self):
        def runner(*args,**kwargs):
            raise FileNotFoundError('binary')
        r=execute_matrix(Path('binary'),Path('rom'),runner=runner)
        self.assertFalse(r['rom_matrix_verified'])
        self.assertEqual(r['error'],'FileNotFoundError')

    def test_bad_timeout_rejected(self):
        with self.assertRaises(ValueError):
            execute_matrix(Path('binary'),Path('rom'),timeout=0)


if __name__ == '__main__':
    unittest.main()
