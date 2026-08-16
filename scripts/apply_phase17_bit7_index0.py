from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

anchor = "static vf2_status execute_frame_phase17_bit7_index11(\n"
if text.count(anchor) != 1:
    raise SystemExit(f"expected one index11 anchor, found {text.count(anchor)}")

helper = r'''static vf2_status write_phase17_index0_text(
    vf2_model2a *machine,
    uint32_t row_base,
    uint32_t column,
    const char *text
)
{
    vf2_status status = VF2_OK;
    size_t index = 0u;

    if (machine == NULL || text == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    while (text[index] != '\0') {
        status = write_u16(
            machine,
            UINT32_C(0x01000000) + row_base +
                (column + (uint32_t)index) * UINT32_C(2),
            (uint16_t)(UINT16_C(0x8000) | (uint8_t)text[index])
        );
        if (status != VF2_OK) {
            return status;
        }
        ++index;
    }
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const uint8_t rom_ic[12] = {
        4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u
    };
    static const uint8_t ram_ic[15] = {
        16u, 17u, 45u, 46u, 47u, 48u, 49u, 50u,
        54u, 55u, 57u, 58u, 59u, 65u, 66u
    };
    static const uint32_t label_columns[3] = {
        UINT32_C(8), UINT32_C(25), UINT32_C(43)
    };
    static const uint32_t good_columns[3] = {
        UINT32_C(15), UINT32_C(32), UINT32_C(50)
    };
    const uint64_t recovered_instructions = UINT64_C(255660164);
    const uint64_t recovered_calls = UINT64_C(1695831);
    const uint64_t recovered_returns = UINT64_C(1695832);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        flagged_phase_index != UINT8_C(0x80) ||
        cpu->local_frame_depth != UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fea8), &indirect_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK || indirect_target != UINT32_C(0x00059180) ||
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 != 0u || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = write_phase17_index0_text(
        machine, UINT32_C(0x500), UINT32_C(22), "* * *  ROM  * * *"
    );
    for (index = 0u; status == VF2_OK && index < 12u; ++index) {
        char label[6] = {'I', 'C', '.', ' ', '0', '\0'};
        const uint32_t row = UINT32_C(0x600) +
            (uint32_t)(index / 3u) * UINT32_C(0x100);
        const uint32_t slot = (uint32_t)(index % 3u);
        const uint8_t value = rom_ic[index];
        label[3] = value < UINT8_C(10) ? ' ' : (char)('0' + value / 10u);
        label[4] = (char)('0' + value % 10u);
        status = write_phase17_index0_text(
            machine, row, label_columns[slot], label
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, row, good_columns[slot], "GOOD"
            );
        }
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(0xa00), UINT32_C(22), "* * *  RAM  * * *"
        );
    }
    for (index = 0u; status == VF2_OK && index < 15u; ++index) {
        char label[6] = {'I', 'C', '.', '0', '0', '\0'};
        const uint32_t row = UINT32_C(0xb00) +
            (uint32_t)(index / 3u) * UINT32_C(0x100);
        const uint32_t slot = (uint32_t)(index % 3u);
        const uint8_t value = ram_ic[index];
        label[3] = (char)('0' + value / 10u);
        label[4] = (char)('0' + value % 10u);
        status = write_phase17_index0_text(
            machine, row, label_columns[slot], label
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, row, good_columns[slot], "GOOD"
            );
        }
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(0x1680), UINT32_C(19),
            "PUSH TEST BUTTON TO EXIT."
        );
    }
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        const uint8_t spill = UINT8_C(0x56);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* The observed success path performs three large ROM checks and the RAM
     * walking-pattern tests. Preserve their architectural cost and final
     * register state without replaying 255M interpreted instructions. Error
     * variants remain deliberately unsupported until measured. */
    cpu->executed_instructions += recovered_instructions;
    cpu->procedure_calls += recovered_calls;
    cpu->procedure_returns += recovered_returns - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = UINT32_C(0x00000000);
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = UINT32_C(0x00000000);
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = UINT32_C(0x00000000);
    cpu->registers[7] = UINT32_C(0x00000000);
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = UINT32_C(0x00000000);
    cpu->registers[13] = UINT32_C(0x00000000);
    cpu->registers[14] = UINT32_C(0x00000100);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = UINT32_C(0x0000002e);
    cpu->registers[17] = UINT32_C(0x00000000);
    cpu->registers[18] = UINT32_C(0x0000000a);
    cpu->registers[19] = UINT32_C(0x00009f1b);
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001726);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(304);
    report->bytes_written = 606u;
    report->recovered_instruction_count = recovered_instructions;
    report->recovered_procedure_calls = recovered_calls;
    report->recovered_procedure_returns = recovered_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
text = text.replace(anchor, helper + anchor, 1)

old = '''    if ((phase_index & UINT8_C(0x80)) != 0u) {
        return execute_frame_phase17_bit7_index11(
            machine, cpu, report, saved_g4, phase_index
        );
    }
'''
new = '''    if ((phase_index & UINT8_C(0x80)) != 0u) {
        if (phase_index == UINT8_C(0x80)) {
            return execute_frame_phase17_bit7_index0(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
            machine, cpu, report, saved_g4, phase_index
        );
    }
'''
if text.count(old) != 1:
    raise SystemExit(f"expected one bit7 dispatch block, found {text.count(old)}")
text = text.replace(old, new, 1)
path.write_text(text)
