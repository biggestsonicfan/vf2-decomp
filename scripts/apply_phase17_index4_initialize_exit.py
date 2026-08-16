from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
if 'finish_frame_phase17_index4_initialize' in text:
    raise SystemExit('initialize/exit recovery already present')

# Correct the ROM-observed global-register residue for the generic EXIT/nav path.
old = '    cpu->registers[9] = UINT32_C(0x00008800);\n'
pos = text.find('static vf2_status finish_frame_phase17_index4_observed(')
end = text.find('static vf2_status finish_frame_phase17_index4_match_count(', pos)
chunk = text[pos:end]
if old not in chunk:
    raise SystemExit('generic index4 r9 anchor not found')
chunk = chunk.replace(old, '    cpu->registers[9] = UINT32_MAX;\n', 1)
text = text[:pos] + chunk + text[end:]

execute_marker = 'static vf2_status execute_frame_phase17_bit7_index4(\n'
helpers = r'''static vf2_status finish_frame_phase17_index4_initialize(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(16941)
        : (edit_delta < 0 ? UINT64_C(16938) : UINT64_C(3044));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(43);
    uint32_t descriptor = 0u;
    uint32_t cursor = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005b344) + UINT32_C(15 * 8), &descriptor
    );
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, descriptor, &cursor);
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
    cpu->registers[9] = UINT32_MAX;
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

static vf2_status finish_frame_phase17_index4_exit_control(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta < 0 ? UINT64_C(3041) : UINT64_C(3044);
    const uint64_t calls = UINT64_C(38);
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
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta < 0 ? UINT32_MAX : 0u;
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
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

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

static vf2_status finish_frame_phase17_index4_exit_taken(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint64_t characters
)
{
    vf2_status status = VF2_OK;
    cpu->executed_instructions += UINT64_C(17317);
    cpu->procedure_calls += UINT64_C(54);
    cpu->procedure_returns += UINT64_C(54);
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
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = UINT32_C(0x00078cb0);
    cpu->registers[17] = 0u;
    cpu->registers[18] = 0u;
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
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
    report->rows = characters;
    report->recovered_instruction_count = UINT64_C(17317);
    report->recovered_procedure_calls = UINT64_C(54);
    report->recovered_procedure_returns = UINT64_C(55);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
if execute_marker not in text:
    raise SystemExit('index4 execute marker missing')
text = text.replace(execute_marker, helpers + execute_marker, 1)

old_edit = '''    if (((phase_a5 >= UINT8_C(1) && phase_a5 <= UINT8_C(3)) ||
         phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11) ||
         packed_bit >= 0) &&'''
new_edit = '''    if (((phase_a5 <= UINT8_C(3)) || phase_a5 == UINT8_C(10) ||
         phase_a5 == UINT8_C(11) || phase_a5 == UINT8_C(15) ||
         packed_bit >= 0) &&'''
if old_edit not in text:
    raise SystemExit('editable-set anchor missing')
text = text.replace(old_edit, new_edit, 1)

old_guard = '''    if (navigation_delta == 0 && phase_a5 > UINT8_C(3) &&
        phase_a5 != UINT8_C(10) && phase_a5 != UINT8_C(11) &&
        packed_bit < 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
'''
new_guard = '''    if (navigation_delta == 0 && phase_a5 > UINT8_C(3) &&
        phase_a5 != UINT8_C(10) && phase_a5 != UINT8_C(11) &&
        phase_a5 != UINT8_C(15) && packed_bit < 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
'''
if old_guard not in text:
    raise SystemExit('unsupported guard anchor missing')
text = text.replace(old_guard, new_guard, 1)

render_anchor = '''    status = phase17_index4_render_descriptors(
        machine, phase_a5, navigation_delta, &characters
    );
'''
body = render_anchor + r'''    if (status == VF2_OK && phase_a5 == UINT8_C(15) && edit_delta != 0) {
        static const uint8_t defaults[29] = {
            2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            15, 0, 0, 160, 0, 200, 0, 64, 64, 64, 37, 37, 37, 31
        };
        uint32_t base = 0u;
        uint16_t crc = 0u;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3340), defaults, sizeof(defaults)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03340), defaults, sizeof(defaults)
            );
        }
        if (status == VF2_OK) {
            status = phase17_index3_rebuild_transfer_tables(
                machine, defaults + UINT32_C(22)
            );
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
        return finish_frame_phase17_index4_initialize(
            machine, cpu, report, edit_delta, crc, characters
        );
    }
    if (status == VF2_OK && phase_a5 == UINT8_C(0) && edit_delta > 0) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
        };
        const uint8_t phase_index = UINT8_C(4);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t restored_characters = 0u;
        size_t index = 0u;

        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index, sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + UINT32_C(32), &record
            );
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, record, &destination);
        if (status == VF2_OK && destination < UINT32_C(4)) return VF2_ERROR_UNSUPPORTED;
        if (status == VF2_OK) {
            status = write_u16(machine, destination - UINT32_C(4), UINT16_C(0x801c));
        }
        for (index = 0u; status == VF2_OK && index < 12u; ++index) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_read_u32(machine, extra_text_records[index], &record);
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
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
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_exit_taken(
            machine, cpu, report, restored_characters
        );
    }
'''
if render_anchor not in text:
    raise SystemExit('render anchor missing')
text = text.replace(render_anchor, body, 1)

final_anchor = '''    if (phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11)) {
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, 0, 0u, characters
        );
    }
'''
final_new = final_anchor + '''    if (phase_a5 == UINT8_C(15)) {
        return finish_frame_phase17_index4_initialize(
            machine, cpu, report, 0, 0u, characters
        );
    }
    if (phase_a5 == UINT8_C(0)) {
        return finish_frame_phase17_index4_exit_control(
            machine, cpu, report, edit_delta, characters
        );
    }
'''
if final_anchor not in text:
    raise SystemExit('final special anchor missing')
text = text.replace(final_anchor, final_new, 1)

path.write_text(text)
