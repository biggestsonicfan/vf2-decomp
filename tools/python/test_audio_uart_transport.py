"""ROM-free observer tests plus optional original user-owned ROM verification."""
import json
import os
import struct
import unittest
from pathlib import Path
from audio_field_xrefs import reconstruct_program
from audio_uart_transport import (ROM_WORDS, verify_uart_opcodes,
                                  observe_transport, STAGE, CURSOR, UART,
                                  QUEUE_LO, QUEUE_WRITE, POP_READ, STAGE_WRITE,
                                  BYTE_READ, UART_WRITE, CURSOR_WRITE,
                                  IRQ_CALL, DRAIN_ENTRY, DRAIN_RET, IRQ_RETURN)


def step(i, pc, after):
    return json.dumps({'type':'step','step':i,'ip_before':pc,'ip_after':after})


def mem(i, kind, address, value, width):
    return json.dumps({'type':'memory','step':i,'kind':kind,'address':address,
                       'size':width,'bytes':value.to_bytes(width,'little').hex()})


def stream(command=0x00AD2342, *, selected=True, slot=QUEUE_LO, bad_byte=False,
           wrong_cursor=False, missing_pop=False, wrong_uart_pc=False,
           repeat_slot=False, partial_writer=False, final_return=True):
    lines=[]
    i=0
    def push(pc, after, event=None):
        nonlocal i
        i+=1
        if event:
            lines.append(mem(i,*event))
        lines.append(step(i,pc,after))
        return i
    width=2 if partial_writer else 4
    writer_step=push(QUEUE_WRITE,0x4392c,('write',slot,command & ((1<<(8*width))-1),width))
    if repeat_slot:
        writer_step=push(QUEUE_WRITE,0x4392c,('write',slot,command,4))
    for b in range(3):
        push(IRQ_CALL,DRAIN_ENTRY)
        if b==0 and not missing_pop:
            push(POP_READ,0x43834,('read',slot,command,4))
            push(STAGE_WRITE,0x4383c,('write',STAGE,command,4))
        byte=(command>>(8*(2-b)))&255
        push(BYTE_READ,0x43860,('read',STAGE+2-b,byte,1))
        push(0x43860,UART_WRITE)
        push(UART_WRITE if not wrong_uart_pc else 0x4386c,0x4386c,
             ('write',UART,byte ^ (1 if bad_byte and b==1 else 0),2))
        push(0x4386c,CURSOR_WRITE)
        push(CURSOR_WRITE,0x43878,('write',CURSOR,(1,0,255)[b] ^ (1 if wrong_cursor and b==1 else 0),1))
        if final_return or b<2:
            push(DRAIN_RET,IRQ_RETURN)
    return lines, (writer_step,slot,command) if selected else None


class TestAudioUartTransport(unittest.TestCase):
    def test_opcode_anchor_accepts_fixture(self):
        image=bytearray(0x44000)
        for pc,value in ROM_WORDS.items():
            struct.pack_into('<I',image,pc,value)
        self.assertEqual(verify_uart_opcodes(image)['verified_rom_words'],len(ROM_WORDS))

    @unittest.skipUnless(os.environ.get('VF2_ROM_ZIP'),'user ROM not supplied')
    def test_actual_rom_opcode_anchors(self):
        self.assertEqual(verify_uart_opcodes(reconstruct_program(Path(os.environ['VF2_ROM_ZIP'])))['verified_rom_words'],len(ROM_WORDS))

    def test_corrupt_opcode_refused(self):
        image=bytearray(0x44000)
        for pc,value in ROM_WORDS.items():
            struct.pack_into('<I',image,pc,value)
        struct.pack_into('<I',image,UART_WRITE,0)
        with self.assertRaises(ValueError):
            verify_uart_opcodes(image)

    def test_exact_filer_to_uart_and_selected(self):
        lines,selected=stream()
        report=observe_transport(lines,selected_writes={selected})
        self.assertEqual(report['counts']['i960_ring_to_uart_complete'],1)
        self.assertEqual(report['counts']['verified_selector_to_uart'],1)
        self.assertEqual(report['witnesses'][0]['uart_bytes'],['0xad','0x23','0x42'])
        self.assertFalse(report['natural_gameplay_sound_proven'])

    def test_unselected_filer_still_transport_not_combat(self):
        lines,_=stream(selected=False)
        result=observe_transport(lines)
        self.assertEqual(result['counts']['i960_ring_to_uart_complete'],1)
        self.assertNotIn('verified_selector_to_uart',result['counts'])

    def test_queue_slot_is_not_value_only(self):
        lines,selected=stream(slot=QUEUE_LO+4)
        result=observe_transport(lines,selected_writes={(selected[0],QUEUE_LO,selected[2])})
        self.assertEqual(result['counts']['i960_ring_to_uart_complete'],1)
        self.assertNotIn('verified_selector_to_uart',result['counts'])

    def test_wrong_second_uart_byte_refused(self):
        lines,selected=stream(bad_byte=True)
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])
        self.assertIn('uart_byte_mismatch_or_missing_read',result['counts'])

    def test_missing_queue_pop_refused(self):
        lines,selected=stream(missing_pop=True)
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])

    def test_unknown_uart_writer_refused(self):
        lines,selected=stream(wrong_uart_pc=True)
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])

    def test_partial_ring_writer_refused(self):
        lines,_=stream(partial_writer=True)
        result=observe_transport(lines)
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])

    def test_bad_cursor_refused(self):
        lines,selected=stream(wrong_cursor=True)
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])

    def test_missing_third_isr_return_refused(self):
        lines,selected=stream(final_return=False)
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])

    def test_overwritten_ring_slot_new_source_wins(self):
        lines,selected=stream(repeat_slot=True)
        result=observe_transport(lines,selected_writes={selected})
        self.assertEqual(result['counts']['unconsumed_slot_overwritten'],1)
        self.assertEqual(result['counts']['verified_selector_to_uart'],1)

    def test_gap_drops_provenance(self):
        lines,selected=stream()
        idx=next(i for i,x in enumerate(lines) if '"ip_before": 5892' in x) if False else 1
        data=json.loads(lines[1]);data['step']=data['step']+3;lines[1]=json.dumps(data)
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])
        self.assertGreater(result['counts'].get('trace_step_gaps',0),0)

    def test_no_irq_call_refused(self):
        lines,selected=stream()
        lines=[x for x in lines if not ('"ip_before": 3620' in x and '"type": "step"' in x)]
        result=observe_transport(lines,selected_writes={selected})
        self.assertNotIn('i960_ring_to_uart_complete',result['counts'])

if __name__=='__main__':
    unittest.main()
