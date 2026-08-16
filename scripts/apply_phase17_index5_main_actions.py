from pathlib import Path

p = Path("src/recovered/texture_bridge_match.c")
s = p.read_text()
a = s.index("static vf2_status execute_frame_phase17_bit7_index5(")
b = s.index("static vf2_status execute_frame_phase17_bit7_index11(", a)
f = s[a:b]


def one(old: str, new: str, label: str) -> None:
    global f
    count = f.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one occurrence, got {count}")
    f = f.replace(old, new, 1)


one(
    """    if (phase_a5 == 0u && edit_delta != 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(credits); ++index) {""",
    """    for (index = 0u; index < sizeof(credits); ++index) {""",
    "remove exit reject",
)

entry_marker = """    if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff)) {"""
entry_block = """    if (phase_a5 == UINT8_C(5) && phase_a7 == UINT8_C(0xff) && edit_delta != 0) {
        const uint8_t manual_state = 0u;
        status = write_phase17_index0_text(
            machine, UINT32_C(44 * 0x80), UINT32_C(20),
            "SELECT BY SERVICE BUTTON"
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(45 * 0x80), UINT32_C(22),
                "AND PUSH TEST BUTTON"
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a7), &manual_state,
                sizeof(manual_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) return status;

        instructions = edit_delta > 0 ? UINT64_C(14063) : UINT64_C(14060);
        calls = UINT64_C(36);
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
        cpu->registers[14] = UINT32_C(1);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(62);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x01001580);
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
        report->rows = UINT64_C(44);
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(0) && phase_a7 == UINT8_C(0xff) && edit_delta > 0) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
        };
        const uint8_t phase_index = UINT8_C(5);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t restored_characters = 0u;
        size_t restore_index = 0u;

        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + UINT32_C(40), &record
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
        for (restore_index = 0u; status == VF2_OK && restore_index < 12u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)restore_index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 3u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[restore_index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x010005d4)
            );
        }
        if (status != VF2_OK) return status;

        instructions = UINT64_C(19056);
        calls = UINT64_C(74);
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
        cpu->registers[14] = UINT32_C(1);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(15);
        cpu->registers[19] = 0u;
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
        report->rows = restored_characters;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

""" + entry_marker
one(entry_marker, entry_block, "main action prelude")

one(
    """    if (status == VF2_OK && main_navigation_delta != 0) {""",
    """    if (status == VF2_OK && phase_a5 == UINT8_C(0) && edit_delta < 0) {
        instructions = UINT64_C(4185);
        calls = UINT64_C(35);
    } else if (status == VF2_OK && main_navigation_delta != 0) {""",
    "exit-minus action",
)

one(
    """    cpu->registers[16] = main_navigation_delta != 0
        ? (main_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1))
        : (edit_delta == 0 ? 0u : (uint32_t)checksum);""",
    """    cpu->registers[16] =
        (phase_a5 == UINT8_C(0) && edit_delta < 0)
            ? UINT32_MAX
            : (main_navigation_delta != 0
                ? (main_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1))
                : (edit_delta == 0 ? 0u : (uint32_t)checksum));""",
    "exit-minus r16",
)

one(
    """    if (main_navigation_delta != 0) {
        static const uint32_t nav_cc_forward[6] = {1u, 1u, 1u, 1u, 2u, 4u};""",
    """    if (phase_a5 == UINT8_C(0) && edit_delta < 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else if (main_navigation_delta != 0) {
        static const uint32_t nav_cc_forward[6] = {1u, 1u, 1u, 1u, 2u, 4u};""",
    "exit-minus condition",
)

s = s[:a] + f + s[b:]
p.write_text(s)
