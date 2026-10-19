"""ROM-free A0 row validation and optional user's actual VF2.1 zip."""
import os
import unittest
import struct
from pathlib import Path
from audio_a0_rows import (
    AUDIO_BASE, AUDIO_SIZE, A0_ROWS, MAIN_BASE, MAIN_SIZE, SELECTOR_TABLE,
    InvalidRom, resolve_command, inspect, read_pieces
)


def fixture():
    main = bytearray(MAIN_SIZE)
    audio = bytearray(AUDIO_SIZE)
    base_row = MAIN_BASE + 0x1e700
    for i in range(1, 19):
        struct.pack_into('<I', main, SELECTOR_TABLE-MAIN_BASE+i*4, base_row+i*44)
    struct.pack_into('<I', main, base_row+44-MAIN_BASE, 0x00ad2342)
    struct.pack_into('<I', main, base_row+44+4-MAIN_BASE, 0x00ad2401)
    for idx, row in ((0x23,0x609500), (0x24,0x609700)):
        struct.pack_into('>I', audio, A0_ROWS-AUDIO_BASE+idx*4,row)
        struct.pack_into('>H', audio,row-AUDIO_BASE,0x7f)
    struct.pack_into('>I',audio,0x609500-AUDIO_BASE+2+0x42*4,0x60b4fe)
    struct.pack_into('>I',audio,0x609700-AUDIO_BASE+2+1*4,0x60b7f2)
    return main,audio


class TestAudioA0Rows(unittest.TestCase):
    def test_resolves_both_families(self):
        main,audio=fixture()
        data=inspect(main,audio)
        self.assertEqual((data['total'],data['resolved'],data['unique_records']), (2,2,2))
        self.assertEqual(resolve_command(audio,0x00ad2342)['record'],0x60b4fe)
        self.assertEqual(resolve_command(audio,0x00ad2401)['record'],0x60b7f2)

    def test_max_gate_refuses_index(self):
        _,audio=fixture()
        struct.pack_into('>H',audio,0x609500-AUDIO_BASE,0x41)
        r=resolve_command(audio,0x00ad2342)
        self.assertFalse(r['in_bounds'])
        self.assertIsNone(r['record'])

    def test_out_of_bank_pointer_refused(self):
        _,audio=fixture()
        struct.pack_into('>I',audio,0x609500-AUDIO_BASE+2+0x42*4,0x704000)
        with self.assertRaises(InvalidRom):
            resolve_command(audio,0x00ad2342)

    def test_odd_pointer_refused(self):
        _,audio=fixture()
        struct.pack_into('>I',audio,0x609500-AUDIO_BASE+2+0x42*4,0x60b4ff)
        with self.assertRaises(InvalidRom):
            resolve_command(audio,0x00ad2342)

    def test_unknown_family_refused(self):
        _,audio=fixture()
        with self.assertRaises(InvalidRom):
            resolve_command(audio,0x00ad2501)

    def test_null_profile_guard(self):
        main,audio=fixture()
        struct.pack_into('<I',main,SELECTOR_TABLE-MAIN_BASE,0x201e700)
        with self.assertRaises(InvalidRom):
            inspect(main,audio)

    def test_invalid_profile_pointer(self):
        main,audio=fixture()
        struct.pack_into('<I',main,SELECTOR_TABLE-MAIN_BASE+4,0x3000000)
        with self.assertRaises(InvalidRom):
            inspect(main,audio)

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'),'requires user-local supported VF2 ROM zip')
    def test_real_rom_complete_census(self):
        main,audio=read_pieces(Path(os.environ['VF2_ROM_ZIP']))
        result=inspect(main,audio)
        self.assertEqual((result['total'],result['resolved'],result['out_of_max'],
                          result['unique_commands'],result['unique_records']),
                         (107,107,0,98,98))
        choices={(e['profile'],e['column']):e for e in result['entries']}
        self.assertEqual((choices[(1,5)]['row'],choices[(1,5)]['record']),
                         (0x60959c,0x60b4fe))
        self.assertEqual((choices[(9,0)]['row'],choices[(9,0)]['record']),
                         (0x60979e,0x60b7f2))
        self.assertEqual(result['families'], {'0x00ad23':64,'0x00ad24':43})


if __name__ == '__main__':
    unittest.main()
