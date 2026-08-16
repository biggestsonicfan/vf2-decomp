from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()
marker = 'static vf2_status execute_frame_phase17_bit7_index11('
if marker not in s:
    raise SystemExit('index11 marker missing')

block = r'''
static vf2_status phase17_index8_restore_menu(vf2_model2a *machine)
{
    static const uint32_t extra[3] = {
        UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
    };
    const uint8_t phase_index = UINT8_C(8);
    const uint8_t spill = UINT8_C(0x56);
    uint32_t record = 0u;
    uint32_t destination = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint64_t characters = 0u;
    size_t index = 0u;
    vf2_status status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));

    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &phase_index, sizeof(phase_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0005feac) + UINT32_C(64), &record
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, record, &destination);
    }
    if (status == VF2_OK && destination < UINT32_C(4)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, destination - UINT32_C(4), UINT16_C(0x801c)
        );
    }
    for (index = 0u; status == VF2_OK && index < 12u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
            &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_read_u32(machine, extra[index], &record);
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    return status;
}

static void phase17_index8_poststate(
    vf2_i960_cpu *cpu,
    uint32_t g0,
    uint32_t g9,
    vf2_i960_compare_result comparison,
    uint32_t condition_bits
)
{
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
    cpu->registers[16] = g0;
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = g9;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
    cpu->compare_result = comparison;
}

static vf2_status execute_frame_phase17_bit7_index8(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    const uint8_t spill = UINT8_C(0x56);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t counter = 0u;
    uint32_t video_words[2] = {0u, 0u};
    uint32_t geometry_pointer = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    vf2_i960_compare_result comparison = VF2_I960_COMPARE_EQUAL;
    uint32_t condition_bits = UINT32_C(2);
    uint32_t final_g0 = 0u;
    uint32_t final_g9 = UINT32_C(0x010016ac);
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x88) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fee8), &indirect_target
    );
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005ed0c) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(3) || phase_a6 != UINT8_C(0xff) ||
        phase_a7 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(0)) {
        uint8_t next = UINT8_C(1);
        uint32_t bit = 0u;
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = write_phase17_index0_text(machine, UINT32_C(15 * 0x80), UINT32_C(18), "IC.47");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(18 * 0x80), UINT32_C(18), "IC.56");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(21 * 0x80), UINT32_C(18), "IC.60");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(24 * 0x80), UINT32_C(18), "IC.64");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(45 * 0x80), UINT32_C(19), "PLEASE WAIT FOR A WHILE.");
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00918000) + UINT32_C(0xe0), 0u);
        for (bit = 0u; status == VF2_OK && bit < 32u; ++bit) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00918004) + bit * UINT32_C(4),
                UINT32_C(1) << bit
            );
        }
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00918084), 0u);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00501004), &geometry_pointer);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x008000f0), geometry_pointer);
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), UINT32_C(64));
        instructions = UINT64_C(663);
        calls = UINT64_C(7);
        final_g9 = UINT32_C(0x01001726);
    } else if (phase_a5 == UINT8_C(1)) {
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
        if (status != VF2_OK || counter == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (counter == UINT32_C(1)) {
            const uint8_t next = UINT8_C(2);
            status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
            instructions = UINT64_C(37);
        } else {
            --counter;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter);
            instructions = UINT64_C(35);
            comparison = VF2_I960_COMPARE_LESS;
            condition_bits = UINT32_C(4);
        }
        calls = UINT64_C(2);
    } else if (phase_a5 == UINT8_C(2)) {
        static const uint32_t rows[4] = {
            UINT32_C(15), UINT32_C(18), UINT32_C(21), UINT32_C(24)
        };
        uint32_t column_index = 0u;
        uint32_t bit = 0u;
        const uint8_t next = UINT8_C(3);
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00980010), &video_words[0]);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00980014), &video_words[1]);
        for (column_index = 0u; status == VF2_OK && column_index < 2u; ++column_index) {
            const uint32_t column = column_index == 0u ? UINT32_C(27) : UINT32_C(58);
            for (bit = 0u; status == VF2_OK && bit < 4u; ++bit) {
                const char *label = (video_words[column_index] & (UINT32_C(1) << bit)) != 0u
                    ? "BAD " : "GOOD";
                status = write_phase17_index0_text(
                    machine, rows[bit] * UINT32_C(0x80), column, label
                );
            }
        }
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(45 * 0x80), UINT32_C(19), "PUSH TEST BUTTON TO EXIT.");
        instructions = UINT64_C(755);
        calls = UINT64_C(19);
        final_g0 = UINT32_C(0x2e);
        final_g9 = UINT32_C(0x01001726);
        comparison = VF2_I960_COMPARE_GREATER;
        condition_bits = UINT32_C(1);
    } else {
        if ((navigation_flags & UINT32_C(0x04000104)) != 0u) {
            status = phase17_index8_restore_menu(machine);
            instructions = UINT64_C(14308);
            calls = UINT64_C(18);
            final_g0 = UINT32_C(0x00078cb0);
        } else if (navigation_flags == 0u) {
            instructions = UINT64_C(36);
            calls = UINT64_C(2);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) return status;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    phase17_index8_poststate(
        cpu, final_g0, final_g9, comparison, condition_bits
    );

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
s = s.replace(marker, block + marker, 1)
old = '''        if (phase_index == UINT8_C(0x87)) {
            return execute_frame_phase17_bit7_index7(
                machine, cpu, report, phase_index
            );
        }
return execute_frame_phase17_bit7_index11('''
new = '''        if (phase_index == UINT8_C(0x87)) {
            return execute_frame_phase17_bit7_index7(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x88)) {
            return execute_frame_phase17_bit7_index8(
                machine, cpu, report, phase_index
            );
        }
return execute_frame_phase17_bit7_index11('''
if old not in s:
    raise SystemExit('dispatch marker missing')
s = s.replace(old, new, 1)
p.write_text(s)

note = Path('decomp/i960/notes/selector17_index8_tgp_test.md')
note.write_text('''# Selector 17 / bit-7 index 8: TGP TEST\n\nROM slot `0x0005fee8` targets entry `0x0005ed0c`.  The handler uses four\n`a5` states at `0x5ed30`, `0x5ee10`, `0x5ee44` and `0x5ef14`.\n\nThe recovered state machine is now complete for the measured TEST-mode\nconfiguration.  State 0 draws IC.47/56/60/64 and PLEASE WAIT, builds the\n32-entry one-hot table at buffer RAM `0x00918004..0x00918080`, stores the live\ngeometry pointer at `0x008000f0`, arms a 64-frame countdown and advances to\nstate 1.  State 1 decrements the countdown and advances to state 2 at 1.\nState 2 reads video-control words `0x00980010` and `0x00980014`; for bits 0..3\nof each word it renders GOOD when clear and BAD when set, then displays\nPUSH TEST BUTTON TO EXIT and advances to state 3.  State 3 idles or exits via\nmask `0x04000104`, restoring TEST MENU index 8 through the common teardown.\n\nReference measurements from the clean `0x0000a6c0 -> 0x0000a010` boundary:\n\n- state 0 build: 663 instructions / 7 calls / 8 returns;\n- state 1 countdown stay: 35 / 2 / 3;\n- state 1 terminal 1 -> state 2: 37 / 2 / 3;\n- state 2 TGP/video result render: 755 / 19 / 20;\n- state 3 idle: 36 / 2 / 3;\n- state 3 TEST exit: 14,308 / 18 / 19.\n\nThe observed video words in the captured runtime were both `0x33333333`, but\nthe native implementation follows the ROM bit test generically rather than\nhard-coding that snapshot.\n''')
