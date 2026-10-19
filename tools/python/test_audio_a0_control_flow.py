"""ROM-free A0 branch predictors plus optional user-owned real ROM."""
import os
import struct
import unittest
from pathlib import Path
from audio_a0_control_flow import WORDS, check_opcodes, predict
from audio_a0_rows import AUDIO_BASE, A0_ROWS, InvalidRom, read_pieces
from test_audio_a0_rows import fixture as base_fixture


def fixture():
    _,rom=base_fixture()
    struct.pack_into('>I', rom, A0_ROWS-AUDIO_BASE+0x24*4, 0x609900)
    struct.pack_into('>H', rom, 0x609900-AUDIO_BASE, 0x7f)
    for family,first in ((0x23,0x60b1e6),(0x24,0x60b7e6)):
        row=0x609500 if family == 0x23 else 0x609900
        for i in range(128):
            at=first+12*i
            struct.pack_into('>I',rom,row-AUDIO_BASE+2+4*i,at)
            struct.pack_into('>6H',rom,at-AUDIO_BASE,
                             0x8fbb if family == 0x23 else 0x8fba,
                             0x2106 if family == 0x23 else 0x2107,
                             0x009b if family == 0x23 else 0x009a,
                             (i<<8)|0x7f,0xff,0x2f00)
    for pc,value in WORDS.items():
        struct.pack_into('>H',rom,pc,value)
    return rom


class TestA0ControlFlow(unittest.TestCase):
    def test_branch_opcode_fixture(self):
        self.assertEqual(check_opcodes(fixture())['helper_call'],0x1f04)

    def test_first_clear_descriptor_and_offset(self):
        proof=predict(fixture(),0x00ad2342,bytes([0x80,0x80,0,0,0,0]))
        self.assertEqual(proof['branch'],0x1ed4)
        self.assertEqual(proof['descriptor_address'],0x3060)
        self.assertEqual(proof['selected_slot'],2)

    def test_occupied_entries_are_not_conjured_free(self):
        proof=predict(fixture(),0x00ad2401,bytes([0x80]*6))
        self.assertIsNone(proof['selected_slot'])
        self.assertTrue(proof['scan_exhausted'])

    def test_missing_descriptor_status_bytes_rejected(self):
        with self.assertRaises(ValueError):
            predict(fixture(),0x00ad2342,b'\x00')

    def test_mutated_branch_refused(self):
        img=fixture()
        struct.pack_into('>H',img,0x1ea6,0x002c)
        with self.assertRaises(InvalidRom):
            check_opcodes(img)

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'),'user ROM only')
    def test_real_opcode_and_both_families(self):
        _,audio=read_pieces(Path(os.environ['VF2_ROM_ZIP']))
        self.assertEqual(check_opcodes(audio)['verified_words'],len(WORDS))
        for c in (0x00ad2342,0x00ad2401,0x00ad247c):
            proof=predict(audio,c,bytes([0,0,0,0,0,0]))
            self.assertEqual(proof['branch'],0x1ed4)
            self.assertEqual(proof['descriptor_address'],0x3020)

if __name__=='__main__':
    unittest.main()
