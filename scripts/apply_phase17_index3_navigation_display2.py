from pathlib import Path
import re

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()

marker = '''static vf2_status execute_frame_phase17_bit7_index3(
'''
helper = r'''static vf2_status finish_frame_phase17_index3_observed(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint64_t instructions,
    uint64_t calls,
    uint32_t r14,
    uint32_t g0,
    uint32_t g1,
    uint32_t g9,
    uint32_t condition_bits,
    vf2_i960_compare_result compare_result,
    uint64_t changed_values,
    size_t bytes_written
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
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = r14;
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = g0;
    cpu->registers[17] = g1;
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = 0u;
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
    cpu->compare_result = compare_result;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
if helper not in text:
    if marker not in text:
        raise SystemExit('index3 marker not found')
    text = text.replace(marker, helper + marker, 1)

old_validation = '''    if (status != VF2_OK || indirect_target != UINT32_C(0x00059f34) ||
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        released_flags != 0u || previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        (phase_a5 != 0u && phase_a5 != UINT8_C(8)) ||
        phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
'''
new_validation = '''    if (status != VF2_OK || indirect_target != UINT32_C(0x00059f34) ||
        input_flags != UINT32_C(0x0ff7f700) || released_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a6 != UINT8_C(0xff) ||
        !(phase_a5 == 0u || phase_a5 == UINT8_C(2) ||
          phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6) ||
          phase_a5 == UINT8_C(8) || phase_a5 == UINT8_C(9) ||
          phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
'''
if old_validation not in text:
    raise SystemExit('index3 validation block not found')
text = text.replace(old_validation, new_validation, 1)

pattern = re.compile(
    r'''    if \(phase_a5 == UINT8_C\(8\)\) \{.*?^    \}\n\n    /\* 0x60600 builds''',
    re.S | re.M,
)
branches = r'''    if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) ||
        phase_a5 == UINT8_C(6) || phase_a5 == UINT8_C(8)) {
        const uint8_t next_state = (uint8_t)(phase_a5 + UINT8_C(1));
        const uint32_t cursor_address =
            phase_a5 == UINT8_C(2) ? UINT32_C(0x0100152c) :
            phase_a5 == UINT8_C(4) ? UINT32_C(0x010015ac) :
            phase_a5 == UINT8_C(6) ? UINT32_C(0x0100162c) :
                                     UINT32_C(0x010016ac);
        const uint32_t r14 =
            phase_a5 == UINT8_C(2) ? UINT32_C(0x00000102) :
            phase_a5 == UINT8_C(4) ? UINT32_C(0x00000104) :
            phase_a5 == UINT8_C(6) ? UINT32_C(0x00000102) :
                                     UINT32_C(0x00000110);
        static const uint32_t cursors[4] = {
            UINT32_C(0x0100152c), UINT32_C(0x010015ac),
            UINT32_C(0x0100162c), UINT32_C(0x010016ac)
        };
        size_t cursor = 0u;

        if (navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (cursor = 0u; status == VF2_OK && cursor < 4u; ++cursor) {
            status = write_u16(
                machine, cursors[cursor],
                cursors[cursor] == cursor_address
                    ? UINT16_C(0x801c) : UINT16_C(0x0020)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report,
            UINT64_C(87), UINT64_C(4), r14,
            UINT32_C(0x1c), 0u, cursor_address,
            UINT32_C(2), VF2_I960_COMPARE_EQUAL,
            UINT64_C(3), 10u
        );
    }

    if (phase_a5 == UINT8_C(9)) {
        uint8_t next_state = phase_a5;
        uint64_t instructions = UINT64_C(271);
        uint64_t calls = UINT64_C(11);
        uint32_t g0 = UINT32_C(31);
        uint32_t g1 = UINT32_C(0x3f4f5c29);
        uint32_t g9 = UINT32_C(0x010014e2);
        uint32_t condition_bits = 0u;
        vf2_i960_compare_result compare_result = VF2_I960_COMPARE_NONE;

        if (navigation_flags == 0u || navigation_flags == UINT32_C(0x20)) {
            uint32_t base = 0u;
            uint8_t values[7];
            size_t value_index = 0u;
            static const uint32_t destinations[7] = {
                UINT32_C(0x0100153a), UINT32_C(0x010015ba),
                UINT32_C(0x0100163a), UINT32_C(0x01001546),
                UINT32_C(0x010015c6), UINT32_C(0x01001646),
                UINT32_C(0x010014e2)
            };

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            for (value_index = 0u; status == VF2_OK && value_index < 7u;
                 ++value_index) {
                status = vf2_model2a_read(
                    machine, base + UINT32_C(0x3356) + (uint32_t)value_index,
                    &values[value_index], sizeof(values[value_index])
                );
            }
            for (value_index = 0u; status == VF2_OK && value_index < 7u;
                 ++value_index) {
                status = phase17_zero_render_decimal(
                    machine, (int32_t)values[value_index],
                    destinations[value_index]
                );
            }
        } else if (navigation_flags == UINT32_C(0x1000)) {
            next_state = UINT8_C(2);
            instructions = UINT64_C(47);
            calls = UINT64_C(3);
            g0 = UINT32_C(1);
            g9 = UINT32_C(0x010016ac);
            condition_bits = UINT32_C(4);
            compare_result = VF2_I960_COMPARE_LESS;
        } else if (navigation_flags == UINT32_C(0x2000)) {
            next_state = UINT8_C(6);
            instructions = UINT64_C(48);
            calls = UINT64_C(3);
            g0 = UINT32_C(1);
            g9 = UINT32_C(0x010016ac);
            condition_bits = UINT32_C(4);
            compare_result = VF2_I960_COMPARE_LESS;
        } else if (navigation_flags == UINT32_C(0x10) ||
                   navigation_flags == UINT32_C(0x100) ||
                   navigation_flags == UINT32_C(0x200)) {
            next_state = UINT8_C(10);
            instructions =
                navigation_flags == UINT32_C(0x10) ? UINT64_C(52) :
                navigation_flags == UINT32_C(0x100) ? UINT64_C(53) :
                                                       UINT64_C(54);
            calls = UINT64_C(3);
            g0 = 0u;
            g9 = UINT32_C(0x010016ac);
            condition_bits = UINT32_C(2);
            compare_result = VF2_I960_COMPARE_EQUAL;
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK && next_state != phase_a5) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report, instructions, calls,
            UINT32_C(0x00000101), g0, g1, g9,
            condition_bits, compare_result,
            next_state == phase_a5 ? UINT64_C(57) : UINT64_C(2),
            next_state == phase_a5 ? 85u : 2u
        );
    }

    if (phase_a5 == UINT8_C(10)) {
        const uint8_t next_state = UINT8_C(11);

        if (navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (row = 0u; status == VF2_OK && row < UINT32_C(48); ++row) {
            for (column = 0u; status == VF2_OK && column < UINT32_C(64);
                 ++column) {
                uint16_t tile = UINT16_C(0x800f);
                if (row == 0u) {
                    if (column == 0u) tile = UINT16_C(0x8018);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8011);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8019);
                } else if (row == UINT32_C(47)) {
                    if (column == 0u) tile = UINT16_C(0x801a);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8010);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x801b);
                } else {
                    if (column == 0u) tile = UINT16_C(0x8013);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8012);
                }
                status = write_u16(
                    machine,
                    UINT32_C(0x01000000) + row * UINT32_C(0x80) +
                        column * UINT32_C(2),
                    tile
                );
            }
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(2 * 0x80), UINT32_C(23),
                "DISPLAY TEST 2/2"
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(45 * 0x80), UINT32_C(20),
                "PUSH TEST BUTTON TO EXIT"
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report,
            UINT64_C(13927), UINT64_C(6), UINT32_C(0x00000102),
            UINT32_C(0x00078c70), UINT32_C(0x3f4f5c29),
            UINT32_C(0x010016a8), UINT32_C(2),
            VF2_I960_COMPARE_EQUAL, UINT64_C(4453),
            (size_t)UINT32_C(64 * 48 * 2 + 82)
        );
    }

    if (phase_a5 == UINT8_C(11)) {
        const int diagnostic_exit =
            navigation_flags == UINT32_C(4) ||
            navigation_flags == UINT32_C(0x10) ||
            navigation_flags == UINT32_C(0x100) ||
            navigation_flags == UINT32_C(0x04000000);

        if (!diagnostic_exit && navigation_flags != 0u &&
            navigation_flags != UINT32_C(0x200)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (diagnostic_exit) {
            static const uint32_t extra_text_records[3] = {
                UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
                UINT32_C(0x0005ff18)
            };
            const uint8_t phase_index = UINT8_C(3);
            uint32_t record = 0u;
            uint32_t destination = 0u;
            uint32_t last_source = 0u;
            uint32_t last_destination = 0u;
            uint64_t characters = 0u;
            size_t record_index = 0u;
            uint64_t exit_instructions =
                navigation_flags == UINT32_C(0x10)
                    ? UINT64_C(14584) : UINT64_C(14582);

            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a4), &phase_index,
                    sizeof(phase_index)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0005feac) + UINT32_C(24),
                    &record
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, record, &destination
                );
            }
            if (status == VF2_OK && destination < UINT32_C(4)) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination - UINT32_C(4), UINT16_C(0x801c)
                );
            }
            for (record_index = 0u; status == VF2_OK && record_index < 12u;
                 ++record_index) {
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0005feac) +
                        (uint32_t)record_index * UINT32_C(8),
                    &record
                );
                if (status == VF2_OK) {
                    status = phase16_copy_text_record(
                        machine, record, &last_source, &last_destination,
                        &characters
                    );
                }
            }
            for (record_index = 0u; status == VF2_OK && record_index < 3u;
                 ++record_index) {
                status = vf2_model2a_read_u32(
                    machine, extra_text_records[record_index], &record
                );
                if (status == VF2_OK) {
                    status = phase16_copy_text_record(
                        machine, record, &last_source, &last_destination,
                        &characters
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            return finish_frame_phase17_index3_observed(
                machine, cpu, report,
                exit_instructions, UINT64_C(18), UINT32_C(0x00000103),
                UINT32_C(0x00078cb0), 0u, UINT32_C(0x010016ac),
                UINT32_C(2), VF2_I960_COMPARE_EQUAL,
                characters + UINT64_C(4),
                (size_t)UINT32_C(64 * 48 * 2) +
                    (size_t)characters * 2u + 4u
            );
        }

        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report,
            UINT64_C(40), UINT64_C(2), UINT32_C(0x00000103),
            0u, UINT32_C(0x3f4f5c29), UINT32_C(0x010016a8),
            0u, VF2_I960_COMPARE_NONE, UINT64_C(1), 1u
        );
    }

    /* 0x60600 builds'''
text, count = pattern.subn(branches, text, count=1)
if count != 1:
    raise SystemExit(f'index3 state block replacement count={count}')

path.write_text(text)
