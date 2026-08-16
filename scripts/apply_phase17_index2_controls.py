from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
start = text.index("static vf2_status execute_frame_phase17_bit7_index2(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)

helper = r'''static vf2_status execute_frame_phase17_bit7_index2(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const uint32_t base_input = UINT32_C(0x0ff7f700);
    static const uint32_t up_input = UINT32_C(0x0ff7e700);
    static const uint32_t down_input = UINT32_C(0x0ff7d700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t sound_control = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    const uint8_t spill = UINT8_C(0x56);
    int selection_up = 0;
    int selection_down = 0;
    int punch4 = 0;
    int punch8 = 0;
    int diagnostic_exit = 0;
    uint64_t instructions = UINT64_C(1844);
    uint64_t calls = UINT64_C(12);
    const char *status_line = "No.  0   Advertise                      ";
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x82) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005feb8), &indirect_target
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
            machine, UINT32_C(0x00500864), &sound_control
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
    if (status != VF2_OK || indirect_target != UINT32_C(0x00059800) ||
        released_flags != 0u || previous_flags != base_input ||
        selector_mask != UINT32_C(0x00020000) || phase_a5 != 0u ||
        phase_a6 != UINT8_C(0xff) || sound_control == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    selection_up = input_flags == up_input && navigation_flags == 0u;
    selection_down = input_flags == down_input && navigation_flags == 0u;
    punch4 = input_flags == base_input && navigation_flags == UINT32_C(0x10);
    punch8 = input_flags == base_input && navigation_flags == UINT32_C(0x100);
    diagnostic_exit =
        input_flags == base_input && navigation_flags == UINT32_C(4);
    if (!(input_flags == base_input && navigation_flags == 0u) &&
        !selection_up && !selection_down && !punch4 && !punch8 &&
        !diagnostic_exit) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (diagnostic_exit) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
            UINT32_C(0x0005ff18)
        };
        static const uint32_t stop_commands[5] = {
            UINT32_C(0x008e2950), UINT32_C(0x008e2e7f),
            UINT32_C(0x008d2250), UINT32_C(0x00891e32),
            UINT32_C(0x00ae101f)
        };
        const uint8_t phase_index = UINT8_C(2);
        const uint8_t queue_cursor = UINT8_C(8);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t characters = 0u;
        size_t index = 0u;

        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &queue_cursor,
            sizeof(queue_cursor)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &queue_cursor,
                sizeof(queue_cursor)
            );
        }
        for (index = 0u; status == VF2_OK && index < 5u; ++index) {
            status = vf2_model2a_write_u32(
                machine,
                UINT32_C(0x0050402c) + (uint32_t)index * UINT32_C(4),
                stop_commands[index]
            );
        }
        if (status == VF2_OK) {
            status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + UINT32_C(16), &record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
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
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += UINT64_C(14450);
        cpu->procedure_calls += UINT64_C(24);
        cpu->procedure_returns += UINT64_C(24);
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
        cpu->registers[14] = UINT32_C(0x0000010d);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = UINT32_C(0x00009f1b);
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010016ac);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        set_equal_condition(cpu);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = characters;
        report->changed_values = characters + UINT64_C(12);
        report->bytes_written =
            (size_t)UINT32_C(64 * 48 * 2) +
            (size_t)characters * 2u + 27u;
        report->recovered_instruction_count = UINT64_C(14450);
        report->recovered_procedure_calls = UINT64_C(24);
        report->recovered_procedure_returns = UINT64_C(25);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (selection_up) {
        status_line = "No.296   Wo13                           ";
        instructions = UINT64_C(1482);
        calls = UINT64_C(11);
        status = write_u16(
            machine, sound_control + UINT32_C(0x80), UINT16_C(296)
        );
    } else if (selection_down) {
        status_line = "No.  1   stage clear                    ";
        instructions = UINT64_C(1538);
        calls = UINT64_C(11);
        status = write_u16(
            machine, sound_control + UINT32_C(0x80), UINT16_C(1)
        );
    } else if (punch4 || punch8) {
        const uint8_t queue_cursor = UINT8_C(4);
        instructions = punch4 ? UINT64_C(1873) : UINT64_C(1872);
        calls = UINT64_C(13);
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &queue_cursor,
            sizeof(queue_cursor)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &queue_cursor,
                sizeof(queue_cursor)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x0050402c), UINT32_C(0x00ad1001)
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    status = write_phase17_index0_text(
        machine, UINT32_C(23 * 0x80), UINT32_C(15), status_line
    );
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(43 * 0x80), UINT32_C(12),
            "SELECT BY PLAYER-1 SIDE LEVER UP/DOWN"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(44 * 0x80), UINT32_C(8),
            "PUSH PLAYER-1 SIDE PUNCH BUTTON TO MAKE SOUND"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(45 * 0x80), UINT32_C(18),
            "PUSH TEST BUTTON TO EXIT"
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

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
    cpu->registers[14] = UINT32_C(0x0000010d);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = 0u;
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = UINT32_C(0x00009f1b);
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001724);
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
    report->changed_values = UINT64_C(149);
    report->bytes_written = 295u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
text = text[:start] + helper + text[end:]
path.write_text(text)
