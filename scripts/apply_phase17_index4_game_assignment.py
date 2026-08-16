from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
marker = 'static vf2_status execute_frame_phase17_bit7_index11(\n'
if marker not in text:
    raise SystemExit('index11 marker not found')
if 'execute_frame_phase17_bit7_index4(' in text:
    raise SystemExit('index4 recovery already present')

block = r'''static vf2_status phase17_index4_render_descriptors(
    vf2_model2a *machine,
    uint8_t selection,
    int navigation_delta,
    uint64_t *characters
)
{
    const uint32_t table = UINT32_C(0x0005b340);
    uint32_t base = 0u;
    uint32_t record = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint64_t local_characters = 0u;
    uint32_t index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );

    for (index = 0u; status == VF2_OK && index < UINT32_C(16); ++index) {
        uint32_t descriptor = 0u;
        uint32_t label_destination = 0u;
        uint32_t value_destination = 0u;
        uint32_t payload = 0u;
        uint32_t flags = 0u;
        uint32_t string_table = 0u;
        uint32_t value = 0u;

        status = vf2_model2a_read_u32(
            machine, table + index * UINT32_C(8) + UINT32_C(4),
            &descriptor
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor, &label_destination);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(4), &value_destination
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(8), &payload
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(12), &flags
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(16), &string_table
            );
        }
        if (status == VF2_OK) {
            status = copy_diagnostic_text(
                machine, descriptor + UINT32_C(20), label_destination,
                &local_characters
            );
        }
        if (status != VF2_OK || (flags & UINT32_C(0x400)) != 0u) {
            continue;
        }

        switch (flags & UINT32_C(3)) {
        case 0u: {
            uint8_t raw = 0u;
            status = vf2_model2a_read(
                machine, base + payload, &raw, sizeof(raw)
            );
            value = raw;
            break;
        }
        case 1u: {
            uint16_t raw = 0u;
            status = read_u16(machine, base + payload, &raw);
            value = raw;
            break;
        }
        case 2u:
            status = vf2_model2a_read_u32(machine, base + payload, &value);
            break;
        default:
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status != VF2_OK) {
            break;
        }
        if ((flags & UINT32_C(4)) != 0u) {
            const uint32_t shift = (flags >> 3u) & UINT32_C(31);
            value = (value >> shift) & UINT32_C(1);
        }
        if ((flags & UINT32_C(0x100)) != 0u) {
            uint32_t source = 0u;
            status = vf2_model2a_read_u32(
                machine, string_table + value * UINT32_C(4), &source
            );
            if (status == VF2_OK) {
                status = copy_diagnostic_text(
                    machine, source, value_destination, &local_characters
                );
            }
        } else {
            status = phase17_zero_render_decimal(
                machine, (int32_t)value, value_destination
            );
        }
    }

    for (index = 0u; status == VF2_OK && index < UINT32_C(2); ++index) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0005ff14) + index * UINT32_C(4), &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination,
                &local_characters
            );
        }
    }

    if (status == VF2_OK) {
        uint32_t descriptor = 0u;
        uint32_t destination = 0u;
        status = vf2_model2a_read_u32(
            machine,
            table + (uint32_t)selection * UINT32_C(8) + UINT32_C(4),
            &descriptor
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4),
                navigation_delta == 0 ? UINT16_C(0x801c) : UINT16_C(0x8020)
            );
        }
    }
    if (status == VF2_OK && characters != NULL) {
        *characters = local_characters;
    }
    return status;
}

static vf2_status finish_frame_phase17_index4_observed(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int navigation_delta,
    uint64_t instructions,
    uint64_t calls,
    uint64_t characters
)
{
    vf2_status status = VF2_OK;

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
    cpu->registers[9] = UINT32_C(0x00008800);
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = navigation_delta < 0
        ? UINT32_MAX : (uint32_t)navigation_delta;
    cpu->registers[17] = UINT32_C(0x0007ae10);
    cpu->registers[18] = 0u;
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001398);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (navigation_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
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

static vf2_status execute_frame_phase17_bit7_index4(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t table = UINT32_C(0x0005b340);
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t diagnostic_flags = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t selector_mirror = UINT8_C(17);
    uint8_t spill = UINT8_C(0x56);
    int navigation_delta = 0;
    uint64_t instructions = UINT64_C(3044);
    uint64_t calls = UINT64_C(38);
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x84) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fec8), &indirect_target
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
            machine, UINT32_C(0x00500708), &released_flags
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
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00508000), &diagnostic_flags
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
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005a680) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        (diagnostic_flags & (UINT32_C(1) << 14u)) != 0u ||
        phase_a5 > UINT8_C(15) || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (navigation_flags == UINT32_C(0x1000)) {
        navigation_delta = 1;
        instructions = UINT64_C(3050);
        calls = UINT64_C(37);
    } else if (navigation_flags == UINT32_C(0x2000)) {
        navigation_delta = -1;
        instructions = UINT64_C(3048);
        calls = UINT64_C(37);
    } else if (navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Editing/exit handlers are recovered separately.  An idle dispatch is
     * currently accepted only for EXIT, whose handler observes no TEST input. */
    if (navigation_delta == 0 && phase_a5 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = phase17_index4_render_descriptors(
        machine, phase_a5, navigation_delta, &characters
    );
    if (status == VF2_OK && navigation_delta != 0) {
        int candidate = (int)phase_a5;
        uint32_t flags = 0u;
        do {
            uint32_t descriptor = 0u;
            candidate += navigation_delta;
            if (candidate < 0) {
                candidate = 15;
            } else if (candidate > 15) {
                candidate = 0;
            }
            status = vf2_model2a_read_u32(
                machine,
                table + (uint32_t)candidate * UINT32_C(8) + UINT32_C(4),
                &descriptor
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, descriptor + UINT32_C(12), &flags
                );
            }
        } while (status == VF2_OK && (flags & UINT32_C(0x200)) != 0u);
        if (status == VF2_OK) {
            const uint8_t next = (uint8_t)candidate;
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next, sizeof(next)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff680),
            navigation_delta == 0 ? UINT32_C(0x00078cb0)
                                  : (uint32_t)navigation_delta
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    return finish_frame_phase17_index4_observed(
        machine, cpu, report, navigation_delta,
        instructions, calls, characters
    );
}

'''
text = text.replace(marker, block + marker, 1)
old = '''        if (phase_index == UINT8_C(0x83)) {
            return execute_frame_phase17_bit7_index3(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
'''
new = '''        if (phase_index == UINT8_C(0x83)) {
            return execute_frame_phase17_bit7_index3(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x84)) {
            return execute_frame_phase17_bit7_index4(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
'''
if old not in text:
    raise SystemExit('phase17 bit7 dispatch snippet not found')
text = text.replace(old, new, 1)
path.write_text(text)
