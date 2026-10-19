"""Synthetic and user-local original-ROM 90-class HIGH admission checks."""
import os
import unittest
from pathlib import Path
from audio_a0_rows import AUDIO_BASE, InvalidRom, read_pieces
from audio_90_boot_route import COMMANDS, boot_descriptors, route


def fixture():
    s=bytearray(0x80000)
    def be32(a,v):s[a-AUDIO_BASE:a-AUDIO_BASE+4]=v.to_bytes(4,'big')
    def put(a,data):s[a-AUDIO_BASE:a-AUDIO_BASE+len(data)]=data
    be32(0x603066,0x6030e6);be32(0x6030a6,0x603186)
    for idx,channel,mode in [(10,0x0a,0xfe),(11,0x0b,0xff)]:
        a=0x603186+16*(idx-10)
        d=bytearray(16);d[0]=0x81;d[1]=channel;d[2]=mode;d[3]=0xff;d[12]=8
        put(a,d)
    for byte,mode in [(0x78,0xfd),(0x7f,0xff)]:put(0x60c6a6+byte,bytes([mode]))
    for ix,base in [(14,0x6045de),(15,0x6046fe)]:be32(0x6032c6+4*ix,base)
    put(0x6045e4,bytes.fromhex('02 61 e0 00 00 00'))
    put(0x60488a,bytes.fromhex('02 42 e0 00 00 00'))
    put(0x60c3e6+1,b'\x81');put(0x60c3e6+0x42,b'\xd6')
    return s


class TestBoot90Route(unittest.TestCase):
    def test_all_two_synthetic(self):
        data=fixture()
        self.assertEqual(len(boot_descriptors(data)),20)
        expected=[(11,'0xff','0xfd','0x60488a','0xd6'),(10,'0xfe','0xff','0x6045e4','0x81')]
        for command,ref in zip(COMMANDS,expected):
            got=route(data,command)
            self.assertEqual((got['boot_descriptor'],got['boot_descriptor_mode'],
                got['selected_mode'],got['table_entry'],got['loopb_lookup_byte']),ref)
            self.assertEqual(got['next_entry_pc'],'0x0902')

    def test_missing_channel_fails(self):
        data=fixture();data[0x3186+16+1]=0
        with self.assertRaises(InvalidRom):route(data,COMMANDS[0])

    def test_ambiguous_active_channel_fails(self):
        data=fixture();data[0x3186+1]=0x0b
        with self.assertRaises(InvalidRom):route(data,COMMANDS[0])

    def test_90_low_descriptor_not_admitted(self):
        data=fixture();data[0x3186+16+2]=0x01
        with self.assertRaises(InvalidRom):route(data,COMMANDS[0])

    def test_rejected_high_entry_fails(self):
        data=fixture();data[0x488a:0x488c]=b'\x00\xff'
        with self.assertRaises(InvalidRom):route(data,COMMANDS[0])

    def test_rom_pointer_oob(self):
        data=fixture();data[0x30a6:0x30aa]=(0x690000).to_bytes(4,'big')
        with self.assertRaises(InvalidRom):route(data,COMMANDS[0])

    def test_non_followup_packet_rejected(self):
        with self.assertRaises(InvalidRom):route(fixture(),0x9b427f)

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'),'requires user-owned ROM zip')
    def test_original_rom_golden(self):
        _,data=read_pieces(Path(os.environ['VF2_ROM_ZIP']))
        a=route(data,0x9b4278);b=route(data,0x9a017f)
        self.assertEqual((a['boot_descriptor'],a['boot_descriptor_mode'],a['selected_mode'],
                          a['row_base'],a['table_entry'],a['lookup_word'],a['lookup_word_alt'],a['loopb_lookup_byte']),
                         (11,'0xff','0xfd','0x6046fe','0x60488a','0x0242','0xe000','0xd6'))
        self.assertEqual((b['boot_descriptor'],b['boot_descriptor_mode'],b['selected_mode'],
                          b['row_base'],b['table_entry'],b['lookup_word'],b['lookup_word_alt'],b['loopb_lookup_byte']),
                         (10,'0xfe','0xff','0x6045de','0x6045e4','0x0261','0xe000','0x81'))

if __name__=='__main__':unittest.main()
