from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

old = """    if (status != VF2_OK || indirect_target != UINT32_C(0x00059164) ||
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
"""
new = """    if (status != VF2_OK || indirect_target != UINT32_C(0x00059164) ||
        input_flags != UINT32_C(0x0ff7f700) ||
        previous_flags != UINT32_C(0x0ff7f700) ||
"""
if text.count(old) != 1:
    raise SystemExit(f"guard prefix: expected one match, found {text.count(old)}")
text = text.replace(old, new, 1)

anchor = """    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);

        /* 0x59164 uses a5 as a secondary dispatch selector.  The observed
         * second visit selects 0x59358, where 0x00500704 & 0x04000104 is
         * zero and the diagnostic wrapper immediately unwinds. */
"""
replacement = """    if (phase_a5 == UINT8_C(0) && navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (phase_a5 == UINT8_C(1) && navigation_flags == UINT32_C(4)) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
            UINT32_C(0x0005ff18)
        };
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t characters = 0u;
        const uint8_t phase_index = UINT8_C(0);
        const uint8_t spill = UINT8_C(0x56);

        /* TEST-button exit from 0x59358 -> 0x5f140.  The ROM clears the
         * diagnostic plane, removes bit 7 from a4, restores the phase marker,
         * and redraws the twelve standard phase labels plus three extras. */
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac), &record
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

        cpu->executed_instructions += UINT64_C(14308);
        cpu->procedure_calls += UINT64_C(18);
        cpu->procedure_returns += UINT64_C(18);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = UINT32_C(0x00000000);
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
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;

        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = characters;
        report->changed_values = characters + UINT64_C(4);
        report->bytes_written =
            (size_t)UINT32_C(64 * 48 * 2) +
            (size_t)characters * 2u + 5u;
        report->recovered_instruction_count = UINT64_C(14308);
        report->recovered_procedure_calls = UINT64_C(18);
        report->recovered_procedure_returns = UINT64_C(19);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(1) && navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);

        /* 0x59164 uses a5 as a secondary dispatch selector.  The observed
         * second visit selects 0x59358, where 0x00500704 & 0x04000104 is
         * zero and the diagnostic wrapper immediately unwinds. */
"""
if text.count(anchor) != 1:
    raise SystemExit(f"a5 anchor: expected one match, found {text.count(anchor)}")
path.write_text(text.replace(anchor, replacement, 1))
