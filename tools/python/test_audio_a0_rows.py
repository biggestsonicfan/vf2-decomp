"""ROM-free A0 row validation and optional user's actual VF2.1 zip."""
import os
import unittest
import struct
from pathlib import Path
from audio_a0_rows import (
    AUDIO_BASE, AUDIO_SIZE, A0_ROWS, MAIN_BASE, MAIN_SIZE, SELECTOR_TABLE,
    InvalidRom, resolve_command, inspect, read_pieces, verify_record_banks,
    inspect_record_payloads
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

    def test_256_contiguous_records_fixture(self):
        _,audio=fixture()
        struct.pack_into('>I', audio, A0_ROWS-AUDIO_BASE+0x24*4,0x609900)
        struct.pack_into('>H', audio,0x609900-AUDIO_BASE,0x7f)
        for family,first in ((0x23,0x60b1e6),(0x24,0x60b7e6)):
            row=(0x609500 if family == 0x23 else 0x609900)
            for i in range(128):
                struct.pack_into('>I', audio,row-AUDIO_BASE+2+4*i,first+12*i)
        bank=verify_record_banks(audio)
        self.assertTrue(bank['contiguous'])
        self.assertEqual((bank['record_count'],bank['record_bytes']), (256,12))

    def test_pointer_stride_mismatch_refused(self):
        _,audio=fixture()
        struct.pack_into('>I', audio, A0_ROWS-AUDIO_BASE+0x24*4,0x609900)
        struct.pack_into('>H', audio,0x609900-AUDIO_BASE,0x7f)
        for family,first in ((0x23,0x60b1e6),(0x24,0x60b7e6)):
            row=(0x609500 if family == 0x23 else 0x609900)
            for i in range(128):
                struct.pack_into('>I', audio,row-AUDIO_BASE+2+4*i,first+12*i)
        struct.pack_into('>I',audio,0x609500-AUDIO_BASE+2+4*54,0x60b1e6+12*54+2)
        with self.assertRaises(InvalidRom):
            verify_record_banks(audio)

    def _payload_fixture(self):
        _, audio = fixture()
        struct.pack_into('>I', audio, A0_ROWS-AUDIO_BASE+0x24*4, 0x609900)
        struct.pack_into('>H', audio, 0x609900-AUDIO_BASE, 0x7f)
        for family, base in ((0x23, 0x60b1e6), (0x24, 0x60b7e6)):
            row = 0x609500 if family == 0x23 else 0x609900
            for index in range(128):
                address = base + index*12
                struct.pack_into('>I', audio, row-AUDIO_BASE+2+4*index, address)
                word0 = 0x8fbb if family == 0x23 else 0x8fba
                word1 = 0x2106 if family == 0x23 else 0x2107
                word2 = 0x009b if family == 0x23 else 0x009a
                tail = (0x78 if index < 68 else 0x7f) if family == 0x23 else 0x7f
                struct.pack_into('>6H', audio, address-AUDIO_BASE,
                                 word0, word1, word2, (index<<8)|tail,
                                 0xff, 0x2f00)
        return audio

    def test_complete_six_word_pattern(self):
        data = inspect_record_payloads(self._payload_fixture())
        self.assertEqual(data['index_aliases'], [])
        self.assertEqual(data['patterns']['0x00ad23']['index_byte_matches'], 128)
        self.assertEqual(data['patterns']['0x00ad23']['low_byte_spans'],
                         [{'from':0,'through':67,'value':0x78},
                          {'from':68,'through':127,'value':0x7f}])
        self.assertEqual(data['patterns']['0x00ad24']['constant_word_columns']['0'], '0x8fba')

    def test_detects_internal_index_alias_without_rewriting(self):
        audio = self._payload_fixture()
        ptr = 0x60b7e6+124*12
        struct.pack_into('>H', audio, ptr-AUDIO_BASE+6, 0x707f)
        data = inspect_record_payloads(audio)
        self.assertEqual(data['index_aliases'],
                         [{'family':'0x00ad24','selector':124,
                           'embedded_index':112,'record':ptr}])

    def test_detects_extra_tail_transition(self):
        audio=self._payload_fixture()
        ptr=0x60b1e6+25*12
        struct.pack_into('>H',audio,ptr-AUDIO_BASE+6,0x1966)
        spans=inspect_record_payloads(audio)['patterns']['0x00ad23']['low_byte_spans']
        self.assertEqual(len(spans),4)
        self.assertEqual(spans[1],{'from':25,'through':25,'value':0x66})

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'),'requires user-local supported VF2 ROM zip')
    def test_real_rom_complete_census(self):
        main,audio=read_pieces(Path(os.environ['VF2_ROM_ZIP']))
        result=inspect(main,audio)
        banks=verify_record_banks(audio)
        self.assertEqual((banks['record_count'],banks['record_bytes']), (256,12))
        self.assertEqual((banks['banks'][0]['first_record'],banks['banks'][1]['end_exclusive']), (0x60b1e6,0x60bde6))
        self.assertEqual((result['total'],result['resolved'],result['out_of_max'],
                          result['unique_commands'],result['unique_records']),
                         (107,107,0,98,98))
        choices={(e['profile'],e['column']):e for e in result['entries']}
        self.assertEqual((choices[(1,5)]['row'],choices[(1,5)]['record']),
                         (0x60959c,0x60b4fe))
        self.assertEqual((choices[(9,0)]['row'],choices[(9,0)]['record']),
                         (0x60979e,0x60b7f2))
        self.assertEqual(result['families'], {'0x00ad23':64,'0x00ad24':43})
        patterns=inspect_record_payloads(audio)
        self.assertEqual(patterns['index_aliases'],
                         [{'family':'0x00ad24','selector':124,
                           'embedded_index':112,'record':0x60bdb6}])
        self.assertEqual(patterns['patterns']['0x00ad23']['low_byte_spans'],
                         [{'from':0,'through':67,'value':0x78},
                          {'from':68,'through':127,'value':0x7f}])
        self.assertEqual(patterns['patterns']['0x00ad24']['index_byte_matches'],127)
        self.assertEqual(patterns['patterns']['0x00ad24']['constant_word_columns'],
                         {'0':'0x8fba','1':'0x2107','2':'0x009a',
                          '4':'0x00ff','5':'0x2f00'})


if __name__ == '__main__':
    unittest.main()
