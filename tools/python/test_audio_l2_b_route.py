"""ROM-free regression fixtures plus optional original VF2 zip census."""
import os
import struct
import unittest
from pathlib import Path
from audio_a0_rows import AUDIO_BASE, InvalidRom, read_pieces
from audio_l2_b_route import WORDS, route
from test_audio_a0_control_flow import fixture


def rom():
    img = fixture()
    for address, value in WORDS.items():
        struct.pack_into('>H',img,address,value)
    return img


class TestAudioL2BRoute(unittest.TestCase):
    def test_ad23_b_class(self):
        r=route(rom(),0x00ad2342)
        self.assertEqual(r['stream_byte'],'0xbb')
        self.assertEqual(r['handler'],'0x1ffc')
        self.assertEqual(r['class_table_pc'],'0x1fe8')

    def test_ad24_b_class(self):
        r=route(rom(),0x00ad2401)
        self.assertEqual(r['stream_byte'],'0xba')
        self.assertEqual(r['handler'],'0x1ffc')

    def test_mutated_opcode_rejected(self):
        a=rom()
        struct.pack_into('>H',a,0x1fba,0x4e75)
        with self.assertRaises(InvalidRom): route(a,0x00ad2342)

    def test_mutated_class_table_rejected(self):
        a=rom()
        struct.pack_into('>H',a,0x1fe8,0x4e75)
        with self.assertRaises(InvalidRom): route(a,0x00ad2342)

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'),'set VF2_ROM_ZIP for ROM checks')
    def test_original_rom_routes_both_families(self):
        _,original=read_pieces(Path(os.environ['VF2_ROM_ZIP']))
        for command,stream_byte in ((0x00ad2342,'0xbb'),(0x00ad2401,'0xba')):
            with self.subTest(command=command):
                r=route(original,command)
                self.assertEqual(r['stream_byte'],stream_byte)
                self.assertEqual(r['handler'],'0x1ffc')
                self.assertEqual(r['class_table_pc'],'0x1fe8')

if __name__=='__main__': unittest.main()
