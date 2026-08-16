from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()

marker = 'static vf2_status execute_frame_phase17_bit7_index3(\n'
helpers = r'''static vf2_status phase17_index3_render_values(
    vf2_model2a *machine,
    const uint8_t values[7]
)
{
    static const uint32_t destinations[7] = {
        UINT32_C(0x0100153a), UINT32_C(0x010015ba),
        UINT32_C(0x0100163a), UINT32_C(0x01001546),
        UINT32_C(0x010015c6), UINT32_C(0x01001646),
        UINT32_C(0x010014e2)
    };
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; status == VF2_OK && index < 7u; ++index) {
        status = phase17_zero_render_decimal(
            machine, (int32_t)values[index], destinations[index]
        );
    }
    return status;
}

static vf2_status phase17_index3_rebuild_transfer_tables(
    vf2_model2a *machine,
    const uint8_t values[7]
)
{
    static const uint32_t channel_bases[3] = {
        UINT32_C(0x01810080), UINT32_C(0x01814080),
        UINT32_C(0x01818080)
    };
    uint32_t level = 0u;
    uint32_t entry = 0u;
    uint32_t channel = 0u;
    vf2_status status = VF2_OK;

    for (channel = 0u; status == VF2_OK && channel < 3u; ++channel) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500234) + channel,
            &values[channel], sizeof(values[channel])
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500237) + channel,
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050023a), &values[6], sizeof(values[6])
        );
    }

    for (level = 0u; status == VF2_OK && level < UINT32_C(32); ++level) {
        const uint32_t sample = level * (uint32_t)values[6];
        for (channel = 0u; status == VF2_OK && channel < UINT32_C(3);
             ++channel) {
            uint32_t result = 0u;
            uint16_t stored = 0u;
            if (sample != 0u) {
                result = (uint32_t)values[channel] +
                    (((uint32_t)values[channel + UINT32_C(3)] * sample) >> 8u);
                stored = result >= UINT32_C(256)
                    ? UINT16_MAX : (uint16_t)result;
            }
            for (entry = 0u; status == VF2_OK && entry < UINT32_C(64);
                 ++entry) {
                status = write_u16(
                    machine,
                    channel_bases[channel] + level * UINT32_C(0x180) +
                        entry * UINT32_C(2),
                    stored
                );
            }
        }
    }
    return status;
}

static vf2_status finish_frame_phase17_index3_rgb_observed(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint64_t instructions,
    uint16_t crc,
    uint32_t r14
)
{
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += UINT64_C(18);
    cpu->procedure_returns += UINT64_C(18);
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
    cpu->registers[14] = r14;
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = (uint32_t)crc;
    cpu->registers[17] = 0u;
    cpu->registers[18] = UINT32_C(0x1d);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = 0u;
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x010014e2);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_NONE;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 1u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(18);
    report->recovered_procedure_returns = UINT64_C(19);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
if 'static vf2_status phase17_index3_rebuild_transfer_tables(' not in text:
    if marker not in text:
        raise SystemExit('index3 marker not found')
    text = text.replace(marker, helpers + marker, 1)

old_validation = '''        !(phase_a5 == 0u || phase_a5 == UINT8_C(2) ||
          phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6) ||
          phase_a5 == UINT8_C(8) || phase_a5 == UINT8_C(9) ||
          phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11))) {
'''
new_validation = '''        !(phase_a5 == 0u || phase_a5 == UINT8_C(2) ||
          phase_a5 == UINT8_C(3) || phase_a5 == UINT8_C(4) ||
          phase_a5 == UINT8_C(5) || phase_a5 == UINT8_C(6) ||
          phase_a5 == UINT8_C(7) || phase_a5 == UINT8_C(8) ||
          phase_a5 == UINT8_C(9) || phase_a5 == UINT8_C(10) ||
          phase_a5 == UINT8_C(11))) {
'''
if old_validation not in text:
    raise SystemExit('index3 state validation not found')
text = text.replace(old_validation, new_validation, 1)

insert_marker = '''    if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) ||
'''
rgb_branch = r'''    if (phase_a5 == UINT8_C(3) || phase_a5 == UINT8_C(5) ||
        phase_a5 == UINT8_C(7)) {
        const uint32_t base_input = UINT32_C(0x0ff7f700);
        const uint32_t allowed_input_changes =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
            (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u);
        const uint32_t input_delta = input_flags ^ base_input;
        const uint32_t channel =
            phase_a5 == UINT8_C(3) ? UINT32_C(0) :
            phase_a5 == UINT8_C(5) ? UINT32_C(1) : UINT32_C(2);
        const uint32_t r14 =
            phase_a5 == UINT8_C(5) ? UINT32_C(0x105) : UINT32_C(0x103);
        uint32_t base = 0u;
        uint8_t values[7];
        uint8_t rendered_values[7];
        uint8_t next_state = phase_a5;
        uint16_t crc = 0u;
        uint64_t instructions = UINT64_C(14499);
        size_t value_index = 0u;

        if ((input_delta & ~allowed_input_changes) != 0u ||
            (input_delta != 0u &&
             input_delta != (UINT32_C(1) << 8u) &&
             input_delta != (UINT32_C(1) << 9u) &&
             input_delta != (UINT32_C(1) << 16u) &&
             input_delta != (UINT32_C(1) << 17u))) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (input_delta != 0u && navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (input_delta == 0u &&
            navigation_flags != 0u && navigation_flags != UINT32_C(0x20) &&
            navigation_flags != UINT32_C(0x1000) &&
            navigation_flags != UINT32_C(0x2000) &&
            navigation_flags != UINT32_C(0x10)) {
            return VF2_ERROR_UNSUPPORTED;
        }

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        for (value_index = 0u; status == VF2_OK && value_index < 7u;
             ++value_index) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3356) + (uint32_t)value_index,
                &values[value_index], sizeof(values[value_index])
            );
            rendered_values[value_index] = values[value_index];
        }
        if (status == VF2_OK) {
            status = phase17_index3_render_values(machine, rendered_values);
        }

        if (status == VF2_OK && input_delta == (UINT32_C(1) << 8u)) {
            if (values[channel] != UINT8_MAX) ++values[channel];
            instructions = UINT64_C(14495);
        } else if (status == VF2_OK &&
                   input_delta == (UINT32_C(1) << 9u)) {
            if (values[channel] != 0u) --values[channel];
            instructions = UINT64_C(14495);
        } else if (status == VF2_OK &&
                   input_delta == (UINT32_C(1) << 16u)) {
            if (values[channel + UINT32_C(3)] != UINT8_MAX) {
                ++values[channel + UINT32_C(3)];
            }
            instructions = UINT64_C(14495);
        } else if (status == VF2_OK &&
                   input_delta == (UINT32_C(1) << 17u)) {
            if (values[channel + UINT32_C(3)] > UINT8_C(16)) {
                --values[channel + UINT32_C(3)];
            }
            instructions = UINT64_C(14495);
        }
        values[6] = UINT8_C(31);

        if (navigation_flags == UINT32_C(0x1000)) {
            next_state = (uint8_t)(phase_a5 + UINT8_C(1));
            instructions = UINT64_C(14498);
        } else if (navigation_flags == UINT32_C(0x2000)) {
            next_state = phase_a5 == UINT8_C(3)
                ? UINT8_C(8) : (uint8_t)(phase_a5 - UINT8_C(3));
            instructions = phase_a5 == UINT8_C(3)
                ? UINT64_C(14501) : UINT64_C(14500);
        } else if (navigation_flags == UINT32_C(0x10)) {
            next_state = UINT8_C(10);
            instructions = phase_a5 == UINT8_C(3)
                ? UINT64_C(14501) : UINT64_C(14500);
        }

        if (status == VF2_OK && next_state != phase_a5) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3356) + channel,
                &values[channel], sizeof(values[channel])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3359) + channel,
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x335c), &values[6], sizeof(values[6])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03356) + channel,
                &values[channel], sizeof(values[channel])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03359) + channel,
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d0335c), &values[6], sizeof(values[6])
            );
        }
        if (status == VF2_OK) {
            status = phase17_index3_rebuild_transfer_tables(machine, values);
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x005ff6c0), (uint16_t)values[channel]
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff6c2), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x005ff6c4),
                (uint16_t)values[channel + UINT32_C(3)]
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff6c6), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff6c8), UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            const uint8_t spill_value = UINT8_C(0x56);
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill_value,
                sizeof(spill_value)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_rgb_observed(
            machine, cpu, report, instructions, crc, r14
        );
    }

'''
if rgb_branch not in text:
    if insert_marker not in text:
        raise SystemExit('index3 cursor branch marker not found')
    text = text.replace(insert_marker, rgb_branch + insert_marker, 1)

# Permit measured raw-input variants only for RGB active states; other states
# still require the canonical baseline input.
old = '''    if (status != VF2_OK || indirect_target != UINT32_C(0x00059f34) ||
        input_flags != UINT32_C(0x0ff7f700) || released_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
'''
new = '''    if (status != VF2_OK || indirect_target != UINT32_C(0x00059f34) ||
        (((phase_a5 != UINT8_C(3) && phase_a5 != UINT8_C(5) &&
           phase_a5 != UINT8_C(7)) &&
          input_flags != UINT32_C(0x0ff7f700))) ||
        released_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
'''
if old not in text:
    raise SystemExit('index3 input validation prefix not found')
text = text.replace(old, new, 1)

path.write_text(text)
