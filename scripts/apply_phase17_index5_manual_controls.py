from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()

old = '''    if (status != VF2_OK || indirect_target != UINT32_C(0x0005b558) ||
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
'''

new = '''    if (status != VF2_OK || indirect_target != UINT32_C(0x0005b558) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(5) || phase_a6 != UINT8_C(0xff) || preset != 0u ||
        (coin_flags & ~UINT32_C(2)) != 0u ||
        (phase_a5 < UINT8_C(5) &&
         (phase_a7 != UINT8_C(0xff) || coin_flags != 0u)) ||
        (phase_a5 == UINT8_C(5) && phase_a7 > UINT8_C(4))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (navigation_flags == UINT32_C(0x100)) edit_delta = 1;
    else if (navigation_flags == UINT32_C(0x200)) edit_delta = -1;
    else if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
    if ((phase_a5 == 0u ||
         (phase_a5 == UINT8_C(5) && phase_a7 == 0u)) && edit_delta != 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(credits); ++index) {
        if (credits[index] != UINT8_C(2)) return VF2_ERROR_UNSUPPORTED;
    }

    if (phase_a5 == UINT8_C(5)) {
        static const uint32_t manual_cursor_rows[5] = {
            UINT32_C(38), UINT32_C(10), UINT32_C(17),
            UINT32_C(24), UINT32_C(31)
        };
        uint8_t manual_values[4] = {0u};
        char value_text[32];
        uint32_t value_offset = 0u;

        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3325), manual_values,
            sizeof(manual_values)
        );
        for (index = 0u; status == VF2_OK && index < 4u; ++index) {
            if (manual_values[index] > UINT8_C(8)) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(6 * 0x80), UINT32_C(25), "MANUAL SETTING"
            );
            characters += UINT64_C(14);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(10 * 0x80), UINT32_C(18), "COIN TO CREDIT"
            );
            characters += UINT64_C(14);
        }
        if (manual_values[0] == 0u) {
            (void)snprintf(value_text, sizeof(value_text), "1 COIN  1 CREDIT");
        } else {
            (void)snprintf(
                value_text, sizeof(value_text), "%u COINS 1 CREDIT",
                (unsigned)manual_values[0] + 1u
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(13 * 0x80), UINT32_C(31), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(17 * 0x80), UINT32_C(18), "BONUS ADDER"
            );
            characters += UINT64_C(11);
        }
        if (manual_values[1] == 0u) {
            (void)snprintf(
                value_text, sizeof(value_text), "           NO BONUS ADDER"
            );
        } else {
            (void)snprintf(
                value_text, sizeof(value_text), "%u COINS GIVE 1 EXTRA COIN",
                (unsigned)manual_values[1] + 1u
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(20 * 0x80), UINT32_C(22), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(24 * 0x80), UINT32_C(18),
                "COIN CHUTE #1 MULTIPLIER"
            );
            characters += UINT64_C(24);
        }
        (void)snprintf(
            value_text, sizeof(value_text),
            manual_values[2] == 0u
                ? "1 COIN COUNTS AS 1 COIN "
                : "1 COIN COUNTS AS %u COINS ",
            (unsigned)manual_values[2] + 1u
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(27 * 0x80), UINT32_C(23), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(31 * 0x80), UINT32_C(18),
                "COIN CHUTE #2 MULTIPLIER"
            );
            characters += UINT64_C(24);
        }
        (void)snprintf(
            value_text, sizeof(value_text),
            manual_values[3] == 0u
                ? "1 COIN COUNTS AS 1 COIN "
                : "1 COIN COUNTS AS %u COINS ",
            (unsigned)manual_values[3] + 1u
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(34 * 0x80), UINT32_C(23), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(38 * 0x80), UINT32_C(18), "EXIT"
            );
            characters += UINT64_C(4);
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine,
                UINT32_C(0x01000000) +
                    manual_cursor_rows[phase_a7] * UINT32_C(0x80) +
                    UINT32_C(16 * 2),
                UINT16_C(0x801c)
            );
        }

        if (status == VF2_OK && edit_delta != 0 && phase_a7 != 0u) {
            int next = (int)manual_values[phase_a7 - 1u] + edit_delta;
            uint8_t value = 0u;
            if (next < 0) next = 8;
            else if (next > 8) next = 0;
            value = (uint8_t)next;
            value_offset = UINT32_C(0x3324) + (uint32_t)phase_a7;
            status = vf2_model2a_write(
                machine, base + value_offset, &value, sizeof(value)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + value_offset,
                    &value, sizeof(value)
                );
            }
            coin_flags |= UINT32_C(2);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, base + UINT32_C(0x3320), coin_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x01d03320), coin_flags
                );
            }
            if (status == VF2_OK) {
                status = compute_table_crc16(
                    machine, base + UINT32_C(0x3320), UINT32_C(15), &checksum
                );
            }
            if (status == VF2_OK) {
                status = write_u16(machine, UINT32_C(0x01d03300), checksum);
            }
            instructions = edit_delta > 0 ? UINT64_C(2475) : UINT64_C(2473);
            calls = UINT64_C(20);
        } else {
            instructions = phase_a7 == 0u ? UINT64_C(2264) : UINT64_C(2266);
            calls = UINT64_C(18);
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
        cpu->registers[14] = UINT32_C(2);
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
        cpu->registers[25] = UINT32_C(0x01001150);
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
        report->rows = characters;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    for (index = 0u; status == VF2_OK && index < sizeof(runs) / sizeof(runs[0]); ++index) {
'''

if old not in s:
    raise SystemExit('target block not found')
s = s.replace(old, new, 1)
p.write_text(s)
