from pathlib import Path

source = Path("src/recovered/texture_bridge_match.c")
text = source.read_text()
marker = "static vf2_status execute_frame_phase17_bit7_index11("
if marker not in text:
    raise SystemExit("index11 marker not found")

helper = r'''static vf2_status phase17_index7_render_choices(
    vf2_model2a *machine,
    int yes_selected,
    uint64_t *characters
)
{
    static const uint32_t records[4] = {
        UINT32_C(0x0005ff20), UINT32_C(0x0005ff24),
        UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
    };
    uint32_t record = 0u;
    uint32_t destination = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint64_t local_characters = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        status = vf2_model2a_read_u32(machine, records[index], &record);
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination,
                &local_characters
            );
        }
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(27 * 0x80), UINT32_C(27), "         "
        );
        local_characters += UINT64_C(9);
    }
    for (index = 2u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(machine, records[index], &record);
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination,
                &local_characters
            );
        }
    }

    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        status = vf2_model2a_read_u32(machine, records[index], &record);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            const uint16_t cursor =
                ((index == 0u) == (yes_selected != 0))
                    ? UINT16_C(0x801c) : UINT16_C(0x8020);
            status = write_u16(machine, destination - UINT32_C(4), cursor);
        }
    }
    if (status == VF2_OK && characters != NULL) {
        *characters = local_characters;
    }
    return status;
}

static vf2_status execute_frame_phase17_bit7_index7(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    const uint8_t spill = UINT8_C(0x56);
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    uint64_t characters = 0u;
    int set_equal = 1;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x87) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0005fee0), &indirect_target);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005e848) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(3) || phase_a6 != UINT8_C(0xff) ||
        phase_a7 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(0)) {
        const uint8_t next = UINT8_C(1);
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = phase17_index7_render_choices(machine, 0, &characters);
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        instructions = UINT64_C(711);
        calls = UINT64_C(7);
    } else if (phase_a5 == UINT8_C(1)) {
        if (navigation_flags == 0u) {
            instructions = UINT64_C(49);
            calls = UINT64_C(4);
            cpu->registers[16] = 0u;
        } else if (navigation_flags == UINT32_C(0x1000) || navigation_flags == UINT32_C(0x2000)) {
            const uint8_t next = UINT8_C(2);
            status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
            instructions = navigation_flags == UINT32_C(0x1000) ? UINT64_C(51) : UINT64_C(48);
            calls = UINT64_C(4);
            cpu->registers[16] = navigation_flags == UINT32_C(0x1000) ? UINT32_C(1) : UINT32_MAX;
            set_equal = 0;
        } else {
            /* 0x60b84 cancellation enters the shared destructive/menu teardown;
             * keep it explicit until that branch is recovered here. */
            return VF2_ERROR_UNSUPPORTED;
        }
    } else if (phase_a5 == UINT8_C(2)) {
        const uint8_t next = UINT8_C(3);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0005ff24), &record);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, record, &destination);
        if (status == VF2_OK && destination >= UINT32_C(4)) status = write_u16(machine, destination - UINT32_C(4), UINT16_C(0x8020));
        else if (status == VF2_OK) status = VF2_ERROR_UNSUPPORTED;
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0005ff20), &record);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, record, &destination);
        if (status == VF2_OK && destination >= UINT32_C(4)) status = write_u16(machine, destination - UINT32_C(4), UINT16_C(0x801c));
        else if (status == VF2_OK) status = VF2_ERROR_UNSUPPORTED;
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        instructions = UINT64_C(43);
        calls = UINT64_C(2);
        cpu->registers[16] = 0u;
        cpu->registers[25] = UINT32_C(0x01000bb0);
    } else {
        if (navigation_flags == 0u) {
            instructions = UINT64_C(41);
            calls = UINT64_C(2);
            cpu->registers[16] = 0u;
        } else if (navigation_flags == UINT32_C(0x1000)) {
            const uint8_t next = UINT8_C(0);
            status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
            instructions = UINT64_C(38);
            calls = UINT64_C(2);
            cpu->registers[16] = 0u;
            set_equal = 0;
        } else {
            /* 0x04000104 confirms the actual backup-SRAM clear.  Do not fake
             * the persistent-state mutation; it is the next recovery cut. */
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));
    }
    if (status != VF2_OK) return status;

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
    if (phase_a5 == UINT8_C(0)) cpu->registers[16] = UINT32_C(0x00078cb0);
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    if (phase_a5 != UINT8_C(2)) cpu->registers[25] = UINT32_C(0x010016ac);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (set_equal) {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
text = text.replace(marker, helper + marker, 1)

old = '''        if (phase_index == UINT8_C(0x86)) {
            return execute_frame_phase17_bit7_index6(
                machine, cpu, report, phase_index
            );
        }
return execute_frame_phase17_bit7_index11(
'''
new = '''        if (phase_index == UINT8_C(0x86)) {
            return execute_frame_phase17_bit7_index6(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x87)) {
            return execute_frame_phase17_bit7_index7(
                machine, cpu, report, phase_index
            );
        }
return execute_frame_phase17_bit7_index11(
'''
if old not in text:
    raise SystemExit("bit7 dispatch anchor not found")
text = text.replace(old, new, 1)
source.write_text(text)

note = Path("decomp/i960/notes/selector17_index7_backup_ram_clear.md")
note.write_text(r'''# Selector 17 / bit-7 index 7: BACK UP RAM CLEAR

ROM entry `0x0005e848` is selected by phase index `0x87`.  Its `a5` jump table
contains five states at `0x5e870`, `0x5e924`, `0x5e948`, `0x5e990` and
`0x5ea80`.

This recovery cut covers the complete non-destructive selection UI.  State 0
draws `YES(CLEAR)` and `NO (CANCEL)`, selects NO and advances to state 1.  State
1 idles or accepts either `0x1000`/`0x2000` from helper `0x60b50`, advancing to
state 2.  State 2 swaps the cursor to YES and advances to state 3.  State 3
idles or accepts canonical `0x1000`, returning to state 0 so NO is rebuilt.

Native ROM measurements from the clean `0x0000a6c0` boundary are:

- state 0 build: 711 instructions, 7 calls, 8 returns;
- state 1 idle: 49 / 4 / 5;
- state 1 positive select: 51 / 4 / 5;
- state 1 negative select: 48 / 4 / 5;
- state 2 cursor swap: 43 / 2 / 3;
- state 3 idle: 41 / 2 / 3;
- state 3 positive return-to-NO: 38 / 2 / 3.

The actual destructive confirmation is deliberately not approximated.  At
state 3 the `0x04000104` path calls `0x6001c`, `0x5427c` and `0x5ff7c`, mutating
backup SRAM, its work-RAM mirror, randomized initialization records and CRC
state before displaying `COMPLETE` and entering state 4's countdown.  The state
1 cancellation path also enters the shared TEST MENU teardown.  Both remain
explicitly unsupported until their full persistent-memory/poststate effects are
recovered and strict-differentially validated.
''')
