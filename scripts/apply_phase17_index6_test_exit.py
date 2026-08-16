from pathlib import Path

source = Path("src/recovered/texture_bridge_match.c")
text = source.read_text()
marker = "static vf2_status execute_frame_phase17_bit7_index6("
if marker not in text:
    raise SystemExit("index6 function marker not found")
head, tail = text.split(marker, 1)

old = """    int page_navigation_delta = 0;\n    int fighter_delta = 0;\n    uint64_t instructions = 0u;\n"""
new = """    int page_navigation_delta = 0;\n    int fighter_delta = 0;\n    int exit_requested = 0;\n    uint64_t instructions = 0u;\n"""
if old not in tail:
    raise SystemExit("index6 locals anchor not found")
tail = tail.replace(old, new, 1)

old = """    if ((phase_a5 & UINT8_C(1)) != 0u && navigation_flags == UINT32_C(0x100)) {\n        page_navigation_delta = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u &&\n               navigation_flags == UINT32_C(0x200)) {\n        page_navigation_delta = -1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x1000)) {\n        fighter_delta = 1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x2000)) {\n        fighter_delta = -1;\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n"""
new = """    if ((phase_a5 & UINT8_C(1)) != 0u &&\n        (navigation_flags & UINT32_C(0x04000014)) != 0u) {\n        /* 0x5cb80 checks the shared TEST/exit mask before page navigation. */\n        exit_requested = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u && navigation_flags == UINT32_C(0x100)) {\n        page_navigation_delta = 1;\n    } else if ((phase_a5 & UINT8_C(1)) != 0u &&\n               navigation_flags == UINT32_C(0x200)) {\n        page_navigation_delta = -1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x1000)) {\n        fighter_delta = 1;\n    } else if (phase_a5 == UINT8_C(9) && navigation_flags == UINT32_C(0x2000)) {\n        fighter_delta = -1;\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n"""
if old not in tail:
    raise SystemExit("index6 input classification anchor not found")
tail = tail.replace(old, new, 1)

old = """    if (status != VF2_OK) return status;\n    if (phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6) || phase_a5 == UINT8_C(8)) {\n"""
exit_block = r'''    if (status != VF2_OK) return status;

    if (exit_requested) {
        static const uint64_t exit_instructions[5] = {
            UINT64_C(16037), UINT64_C(17895), UINT64_C(17391),
            UINT64_C(16215), UINT64_C(16173)
        };
        static const uint64_t exit_calls[5] = {
            UINT64_C(95), UINT64_C(144), UINT64_C(115),
            UINT64_C(90), UINT64_C(50)
        };
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
        };
        const uint8_t phase_index = UINT8_C(6);
        const size_t page = (size_t)((phase_a5 - UINT8_C(1)) / UINT8_C(2));
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t restored_characters = 0u;
        size_t restore_index = 0u;

        /* 0x5f140 clears the diagnostic plane, drops bit 7 from a4 and
         * reconstructs the parent TEST MENU from the ROM text records. */
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)phase_index * UINT32_C(8),
                &record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4), UINT16_C(0x801c)
            );
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 12u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)restore_index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 3u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[restore_index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) return status;

        instructions = exit_instructions[page];
        calls = exit_calls[page];
        cpu->executed_instructions += instructions;
        cpu->procedure_calls += calls;
        cpu->procedure_returns += calls;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(5);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010016ac);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;

        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = restored_characters;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6) || phase_a5 == UINT8_C(8)) {
'''
if old not in tail:
    raise SystemExit("index6 post-render anchor not found")
tail = tail.replace(old, exit_block, 1)
source.write_text(head + marker + tail)

notes = Path("decomp/i960/notes/selector17_index6_bookkeeping.md")
body = notes.read_text()
old_note = """The TEST exit mask in the same ROM tail branches directly to the shared teardown\nat `0x0005f140`. That exit remains intentionally outside this cut until its full\ncaller-visible poststate is measured; the recovered bridge continues to reject\nthat input rather than substituting an estimated teardown state.\n"""
new_note = """The TEST exit mask in the same ROM tail is `0x04000014` and has precedence over\npage navigation. The canonical TEST bit (`0x00000004`) was injected only after\nthe clean `0x0000a6c0` frame boundary so the scheduler could not consume it\nbefore the BOOKKEEPING handler. Native ROM measurements from stable states\n`a5=1,3,5,7,9` are respectively 16,037/95/96, 17,895/144/145,\n17,391/115/116, 16,215/90/91, and 16,173/50/51\n(instructions/calls/returns). All five converge on the same caller-visible CPU\npoststate. The shared teardown at `0x0005f140` clears the 64x48 diagnostic tile\nplane, clears bit 7 of `a4` (`0x86 -> 0x06`), restores the twelve parent TEST\nMENU records plus the three common instruction records, and redraws the\nBOOKKEEPING cursor. The recovered bridge now reproduces that teardown and its\nmeasured poststate, completing selector-17 index 6 for the measured empty/default\nbookkeeping configuration.\n"""
if old_note not in body:
    raise SystemExit("bookkeeping exit note anchor not found")
notes.write_text(body.replace(old_note, new_note, 1))
