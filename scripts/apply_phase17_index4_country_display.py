from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
if 'finish_frame_phase17_index4_special_assignment' in text:
    raise SystemExit('country/display recovery already present')

execute_marker = 'static vf2_status execute_frame_phase17_bit7_index4(\n'
helper = r'''static vf2_status finish_frame_phase17_index4_special_assignment(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t selection,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    uint64_t instructions = UINT64_C(3045);
    uint64_t calls = UINT64_C(38);
    uint32_t descriptor = 0u;
    uint32_t cursor = 0u;
    vf2_status status = VF2_OK;

    if (edit_delta != 0) {
        if (selection == UINT8_C(10)) {
            instructions = edit_delta > 0 ? UINT64_C(3427) : UINT64_C(3425);
            calls = UINT64_C(41);
        } else if (selection == UINT8_C(11)) {
            instructions = edit_delta > 0 ? UINT64_C(16890) : UINT64_C(16887);
            calls = UINT64_C(43);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0005b344) + (uint32_t)selection * UINT32_C(8),
        &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &cursor);
    }
    if (status != VF2_OK || cursor < UINT32_C(4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cursor -= UINT32_C(4);

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
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = cursor;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (edit_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
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
if execute_marker not in text:
    raise SystemExit('index4 execute marker not found')
text = text.replace(execute_marker, helper + execute_marker, 1)

old_edit = '''    if (((phase_a5 >= UINT8_C(1) && phase_a5 <= UINT8_C(3)) ||
         packed_bit >= 0) &&
        (navigation_flags == UINT32_C(0x100) ||
         navigation_flags == UINT32_C(0x200))) {'''
new_edit = '''    if (((phase_a5 >= UINT8_C(1) && phase_a5 <= UINT8_C(3)) ||
         phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11) ||
         packed_bit >= 0) &&
        (navigation_flags == UINT32_C(0x100) ||
         navigation_flags == UINT32_C(0x200))) {'''
if old_edit not in text:
    raise SystemExit('edit-input anchor not found')
text = text.replace(old_edit, new_edit, 1)
text = text.replace(
    '    if (navigation_delta == 0 && phase_a5 > UINT8_C(3) && packed_bit < 0) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n',
    '    if (navigation_delta == 0 && phase_a5 > UINT8_C(3) &&\n        phase_a5 != UINT8_C(10) && phase_a5 != UINT8_C(11) &&\n        packed_bit < 0) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n',
    1
)

render_anchor = '''    status = phase17_index4_render_descriptors(
        machine, phase_a5, navigation_delta, &characters
    );
'''
special = render_anchor + r'''    if (status == VF2_OK && phase_a5 == UINT8_C(10) && edit_delta != 0) {
        uint32_t base = 0u;
        uint8_t country = 0u;
        uint8_t flags = 0u;
        uint16_t crc = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3350), &country, sizeof(country)
            );
        }
        if (status == VF2_OK) {
            int next = (int)country + edit_delta;
            if (next < 0) next = 2;
            else if (next > 2) next = 0;
            country = (uint8_t)next;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3350), &country, sizeof(country)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03350), &country, sizeof(country)
            );
        }
        if (status == VF2_OK && country != 0u) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
            if (status == VF2_OK) {
                flags |= UINT8_C(1) << 3u;
                status = vf2_model2a_write(
                    machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d03351), &flags, sizeof(flags)
                );
            }
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) return status;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, edit_delta, crc, characters
        );
    }
    if (status == VF2_OK && phase_a5 == UINT8_C(11) && edit_delta != 0) {
        uint32_t base = 0u;
        uint8_t flags = 0u;
        uint8_t values[7];
        uint16_t crc = 0u;
        size_t index = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            flags ^= UINT8_C(1) << 2u;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03351), &flags, sizeof(flags)
            );
        }
        if ((flags & (UINT8_C(1) << 2u)) != 0u) {
            values[0] = values[1] = values[2] = UINT8_C(117);
            values[3] = values[4] = values[5] = UINT8_C(34);
        } else {
            values[0] = values[1] = values[2] = UINT8_C(64);
            values[3] = values[4] = values[5] = UINT8_C(37);
        }
        values[6] = UINT8_C(31);
        for (index = 0u; status == VF2_OK && index < 7u; ++index) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3356) + (uint32_t)index,
                &values[index], sizeof(values[index])
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d03356) + (uint32_t)index,
                    &values[index], sizeof(values[index])
                );
            }
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
        if (status != VF2_OK) return status;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, edit_delta, crc, characters
        );
    }
'''
if render_anchor not in text:
    raise SystemExit('renderer anchor not found')
text = text.replace(render_anchor, special, 1)

final_anchor = '''    if (packed_bit >= 0) {
        return finish_frame_phase17_index4_packed_flag(
            machine, cpu, report, phase_a5, 0, 0u, characters
        );
    }
'''
final_new = final_anchor + '''    if (phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11)) {
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, 0, 0u, characters
        );
    }
'''
if final_anchor not in text:
    raise SystemExit('final packed anchor not found')
text = text.replace(final_anchor, final_new, 1)

path.write_text(text)
