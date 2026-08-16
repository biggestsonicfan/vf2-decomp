from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
start = text.index("static vf2_status execute_frame_phase17_bit7_index1(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)
block = text[start:end]

old_guard = "phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff))"
new_guard = "phase_a5 > UINT8_C(2) || phase_a6 != UINT8_C(0xff))"
if block.count(old_guard) != 1:
    raise SystemExit(f"phase guard anchor count={block.count(old_guard)}")
block = block.replace(old_guard, new_guard, 1)

anchor = """    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);
"""
if block.count(anchor) != 1:
    raise SystemExit(f"phase1 anchor count={block.count(anchor)}")

third = r'''    if (phase_a5 == UINT8_C(2)) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
            UINT32_C(0x0005ff18)
        };
        const uint8_t spill = UINT8_C(0x56);
        const int diagnostic_exit = released_flags == UINT32_C(4);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t characters = 0u;

        if (diagnostic_exit) {
            const uint8_t phase_index = UINT8_C(1);

            /* 0x597a8 refreshes the INPUT TEST and, on a released TEST bit,
             * branches to the shared diagnostic teardown at 0x5f140. */
            status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a4), &phase_index,
                    sizeof(phase_index)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0005feac) + UINT32_C(8),
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

        cpu->executed_instructions +=
            diagnostic_exit ? UINT64_C(15895) : UINT64_C(1622);
        cpu->procedure_calls +=
            diagnostic_exit ? UINT64_C(53) : UINT64_C(37);
        cpu->procedure_returns +=
            diagnostic_exit ? UINT64_C(53) : UINT64_C(37);
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
        cpu->registers[14] = UINT32_C(0x0000010b);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = diagnostic_exit
            ? UINT32_C(0x00078cb0) : UINT32_C(0x0046464f);
        cpu->registers[17] = diagnostic_exit
            ? UINT32_C(0) : UINT32_C(0x3f4f5c29);
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = UINT32_C(0x00009f1b);
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = diagnostic_exit
            ? UINT32_C(0x010016ac) : UINT32_C(0x010012cc);
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
        report->changed_values = diagnostic_exit
            ? characters + UINT64_C(4) : UINT64_C(3);
        report->bytes_written = diagnostic_exit
            ? (size_t)UINT32_C(64 * 48 * 2) +
                (size_t)characters * 2u + 5u
            : 3u;
        report->recovered_instruction_count =
            diagnostic_exit ? UINT64_C(15895) : UINT64_C(1622);
        report->recovered_procedure_calls =
            diagnostic_exit ? UINT64_C(53) : UINT64_C(37);
        report->recovered_procedure_returns =
            diagnostic_exit ? UINT64_C(54) : UINT64_C(38);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

'''
block = block.replace(anchor, third + anchor, 1)
text = text[:start] + block + text[end:]
path.write_text(text)
