from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
start = text.index('static vf2_status execute_frame_phase17_bit7_index5(')
end = text.index('\nstatic vf2_status execute_frame_phase17_bit7_index11(', start)
new = r'''static vf2_status execute_frame_phase17_bit7_index5(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    static const uint32_t cursor_addresses[5] = {
        UINT32_C(0x01001320), UINT32_C(0x010002a0),
        UINT32_C(0x01000320), UINT32_C(0x01000420),
        UINT32_C(0x010005a0)
    };
    static const struct {
        uint32_t row;
        uint32_t column;
        const char *text;
    } runs[] = {
        {5u, 18u, "COIN CHUTE TYPE"},
        {5u, 35u, "    COMMON"},
        {6u, 18u, "CREDIT TO 1P START"},
        {6u, 40u, "2"},
        {6u, 42u, "CREDITS"},
        {7u, 28u, "1P CONTINUE"},
        {7u, 40u, "2"},
        {7u, 42u, "CREDITS"},
        {8u, 18u, "CREDIT TO VS START"},
        {8u, 40u, "2"},
        {8u, 42u, "CREDITS"},
        {9u, 28u, "VS CONTINUE"},
        {9u, 40u, "2"},
        {9u, 42u, "CREDITS"},
        {11u, 18u, "COIN/CREDIT SETTING     #  1"},
        {11u, 47u, " "},
        {13u, 16u, "COIN CHUTE #1"},
        {13u, 31u, "1 COIN  1 CREDIT "},
        {15u, 31u, "                 "},
        {17u, 31u, "                 "},
        {19u, 31u, "                 "},
        {21u, 31u, "                 "},
        {24u, 16u, "COIN CHUTE #2"},
        {24u, 31u, "1 COIN  1 CREDIT "},
        {26u, 31u, "                 "},
        {28u, 31u, "                 "},
        {30u, 31u, "                 "},
        {32u, 31u, "                 "},
        {35u, 18u, "MANUAL SETTING"},
        {38u, 18u, "EXIT"},
        {44u, 20u, "SELECT BY SERVICE BUTTON"},
        {45u, 22u, "AND PUSH TEST BUTTON"}
    };
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint32_t coin_flags = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    uint8_t preset = 0u;
    uint8_t credits[6] = {0u};
    const uint8_t spill = UINT8_C(0x56);
    int edit_delta = 0;
    uint16_t checksum = 0u;
    uint64_t instructions = UINT64_C(4188);
    uint64_t calls = UINT64_C(35);
    size_t index = 0u;
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x85) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0005fed0), &indirect_target);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, base + UINT32_C(0x3320), &coin_flags);
    if (status == VF2_OK) status = vf2_model2a_read(machine, base + UINT32_C(0x3324), &preset, sizeof(preset));
    if (status == VF2_OK) status = vf2_model2a_read(machine, base + UINT32_C(0x3329), credits, sizeof(credits));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005b558) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(4) || phase_a6 != UINT8_C(0xff) ||
        phase_a7 != UINT8_C(0xff) || coin_flags != 0u || preset != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (navigation_flags == UINT32_C(0x100)) edit_delta = 1;
    else if (navigation_flags == UINT32_C(0x200)) edit_delta = -1;
    else if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
    if (phase_a5 == 0u && edit_delta != 0) return VF2_ERROR_UNSUPPORTED;
    for (index = 0u; index < sizeof(credits); ++index) {
        if (credits[index] != UINT8_C(2)) return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; status == VF2_OK && index < sizeof(runs) / sizeof(runs[0]); ++index) {
        status = write_phase17_index0_text(
            machine, runs[index].row * UINT32_C(0x80), runs[index].column,
            runs[index].text
        );
        if (status == VF2_OK) characters += (uint64_t)strlen(runs[index].text);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, cursor_addresses[phase_a5], UINT16_C(0x801c));
    }

    if (status == VF2_OK && edit_delta != 0) {
        if (phase_a5 == UINT8_C(1)) {
            coin_flags &= ~UINT32_C(2);
            coin_flags ^= UINT32_C(1);
            status = vf2_model2a_write_u32(machine, base + UINT32_C(0x3320), coin_flags);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, UINT32_C(0x01d03320), coin_flags);
            }
            instructions = edit_delta > 0 ? UINT64_C(4405) : UINT64_C(4402);
            calls = UINT64_C(38);
        } else if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(3)) {
            const uint32_t offset = phase_a5 == UINT8_C(2)
                ? UINT32_C(0x3329) : UINT32_C(0x332c);
            uint8_t value = UINT8_C(2);
            uint8_t start_credit = 0u;
            uint8_t continue_credit = 0u;
            int next = (int)value + edit_delta;
            if (next < 0) next = 14;
            else if (next > 14) next = 0;
            value = (uint8_t)next;
            status = vf2_model2a_write(machine, base + offset, &value, sizeof(value));
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + offset, &value, sizeof(value)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x0005bc74) + (uint32_t)value,
                    &start_credit, sizeof(start_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x0005bc84) + (uint32_t)value,
                    &continue_credit, sizeof(continue_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, base + offset + UINT32_C(1),
                    &start_credit, sizeof(start_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + offset + UINT32_C(1),
                    &start_credit, sizeof(start_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, base + offset + UINT32_C(2),
                    &continue_credit, sizeof(continue_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + offset + UINT32_C(2),
                    &continue_credit, sizeof(continue_credit)
                );
            }
            instructions = edit_delta > 0 ? UINT64_C(4405) : UINT64_C(4402);
            calls = UINT64_C(38);
        } else if (phase_a5 == UINT8_C(4)) {
            int next = (int)preset + edit_delta;
            if (next < 0) next = 25;
            else if (next > 25) next = 0;
            preset = (uint8_t)next;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3324), &preset, sizeof(preset)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d03324), &preset, sizeof(preset)
                );
            }
            instructions = edit_delta > 0 ? UINT64_C(4403) : UINT64_C(4401);
            calls = UINT64_C(37);
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3320), UINT32_C(15), &checksum
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03300), checksum);
        }
    } else if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(3)) {
        instructions = UINT64_C(4190);
    } else if (phase_a5 == UINT8_C(4)) {
        instructions = UINT64_C(4193);
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
    cpu->registers[14] = UINT32_C(1);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)checksum;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x3f4f5c29) : 0u;
    cpu->registers[18] = edit_delta == 0 ? UINT32_C(0xc0a0a3d7) : UINT32_C(15);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x010005d4);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (phase_a5 == 0u && edit_delta == 0) {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
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
path.write_text(text[:start] + new + text[end:])
