from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
start = text.index("static vf2_status execute_frame_phase17_bit7_index2(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)
block = text[start:end]

old_guard = """        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        released_flags != 0u || previous_flags != UINT32_C(0x0ff7f700) ||
"""
new_guard = """        input_flags != UINT32_C(0x0ff7f700) ||
        (navigation_flags != 0u && navigation_flags != UINT32_C(4)) ||
        released_flags != 0u || previous_flags != UINT32_C(0x0ff7f700) ||
"""
if block.count(old_guard) != 1:
    raise SystemExit(f"guard anchor count={block.count(old_guard)}")
block = block.replace(old_guard, new_guard, 1)

anchor = """    status = write_phase17_index0_text(
        machine, UINT32_C(23 * 0x80), UINT32_C(15),
"""
if block.count(anchor) != 1:
    raise SystemExit(f"idle anchor count={block.count(anchor)}")

exit_branch = r'''    if (navigation_flags == UINT32_C(4)) {
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
        size_t command_index = 0u;

        /* 0x598b4 handles TEST by issuing the five measured stop commands,
         * then returns non-equal so 0x59804 enters the shared 0x5f140
         * diagnostic teardown. */
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
        for (command_index = 0u; status == VF2_OK && command_index < 5u;
             ++command_index) {
            status = vf2_model2a_write_u32(
                machine,
                UINT32_C(0x0050402c) + (uint32_t)command_index * UINT32_C(4),
                stop_commands[command_index]
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
                machine,
                UINT32_C(0x0005feac) + UINT32_C(16),
                &record
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
        for (command_index = 0u; status == VF2_OK && command_index < 12u;
             ++command_index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) +
                    (uint32_t)command_index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        for (command_index = 0u; status == VF2_OK && command_index < 3u;
             ++command_index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[command_index], &record
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

'''
block = block.replace(anchor, exit_branch + anchor, 1)
text = text[:start] + block + text[end:]
path.write_text(text)
