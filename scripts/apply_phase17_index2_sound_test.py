from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
anchor = "static vf2_status execute_frame_phase17_bit7_index11("
if text.count(anchor) != 1:
    raise SystemExit(f"index11 anchor count={text.count(anchor)}")

helper = r'''static vf2_status execute_frame_phase17_bit7_index2(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    const uint8_t spill = UINT8_C(0x56);
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
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        released_flags != 0u || previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) || phase_a5 != 0u ||
        phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = write_phase17_index0_text(
        machine, UINT32_C(23 * 0x80), UINT32_C(15),
        "No.  0   Advertise                      "
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

    cpu->executed_instructions += UINT64_C(1844);
    cpu->procedure_calls += UINT64_C(12);
    cpu->procedure_returns += UINT64_C(12);
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
    report->recovered_instruction_count = UINT64_C(1844);
    report->recovered_procedure_calls = UINT64_C(12);
    report->recovered_procedure_returns = UINT64_C(13);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
text = text.replace(anchor, helper + anchor, 1)

route = """        if (phase_index == UINT8_C(0x81)) {
            return execute_frame_phase17_bit7_index1(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
"""
route2 = """        if (phase_index == UINT8_C(0x81)) {
            return execute_frame_phase17_bit7_index1(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x82)) {
            return execute_frame_phase17_bit7_index2(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
"""
if text.count(route) != 1:
    raise SystemExit(f"route anchor count={text.count(route)}")
text = text.replace(route, route2, 1)
path.write_text(text)
