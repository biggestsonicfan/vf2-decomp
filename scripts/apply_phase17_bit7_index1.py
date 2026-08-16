from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

anchor = "static vf2_status execute_frame_phase17_bit7_index11(\n"
if text.count(anchor) != 1:
    raise SystemExit(f"expected one index11 anchor, found {text.count(anchor)}")

helper = r'''static vf2_status execute_frame_phase17_bit7_index1(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const struct {
        uint32_t row;
        uint32_t column;
        const char *text;
    } lines[] = {
        {UINT32_C(5),  UINT32_C(20), "PLAYER      1P       2P"},
        {UINT32_C(8),  UINT32_C(20), "UP      :   ON       ON"},
        {UINT32_C(10), UINT32_C(20), "DOWN    :   ON       ON"},
        {UINT32_C(12), UINT32_C(20), "RIGHT   :   ON       ON"},
        {UINT32_C(14), UINT32_C(20), "LEFT    :   ON       ON"},
        {UINT32_C(18), UINT32_C(20), "PUNCH   :   ON       ON"},
        {UINT32_C(20), UINT32_C(20), "KICK    :   ON       ON"},
        {UINT32_C(22), UINT32_C(20), "GUARD   :   ON       ON"},
        {UINT32_C(26), UINT32_C(20), "START   :   OFF      OFF"},
        {UINT32_C(30), UINT32_C(23), "COIN CHUTE 1 : OFF"},
        {UINT32_C(32), UINT32_C(23), "COIN CHUTE 2 : OFF"},
        {UINT32_C(34), UINT32_C(23), "SERVICE SW   : OFF"},
        {UINT32_C(36), UINT32_C(23), "TEST SW      : OFF"},
        {UINT32_C(45), UINT32_C(20), "PUSH TEST BUTTON TO EXIT"}
    };
    uint32_t primary_target = 0u;
    uint32_t secondary_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    size_t index = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        flagged_phase_index != UINT8_C(0x81) ||
        cpu->local_frame_depth != UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005feb0), &primary_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0005972c), &secondary_target
        );
    }
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
    if (status != VF2_OK ||
        primary_target != UINT32_C(0x00059718) ||
        secondary_target != UINT32_C(0x00059738) ||
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 != 0u || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (index = 0u; status == VF2_OK &&
         index < sizeof(lines) / sizeof(lines[0]); ++index) {
        status = write_phase17_index0_text(
            machine,
            lines[index].row * UINT32_C(0x80),
            lines[index].column,
            lines[index].text
        );
        if (status == VF2_OK) {
            bytes_written += strlen(lines[index].text) * sizeof(uint16_t);
        }
    }
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
    }
    if (status == VF2_OK) {
        const uint8_t spill = UINT8_C(0x56);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += UINT64_C(3316);
    cpu->procedure_calls += UINT64_C(51);
    cpu->procedure_returns += UINT64_C(51);
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
    cpu->registers[14] = UINT32_C(0x00000109);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = UINT32_C(0x0046464f);
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = UINT32_C(0x00009f1b);
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x010012cc);
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
    report->changed_values = UINT64_C(205);
    report->bytes_written = bytes_written + 4u;
    report->recovered_instruction_count = UINT64_C(3316);
    report->recovered_procedure_calls = UINT64_C(51);
    report->recovered_procedure_returns = UINT64_C(52);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
text = text.replace(anchor, helper + anchor, 1)

old = '''        if (phase_index == UINT8_C(0x80)) {
            return execute_frame_phase17_bit7_index0(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
'''
new = '''        if (phase_index == UINT8_C(0x80)) {
            return execute_frame_phase17_bit7_index0(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x81)) {
            return execute_frame_phase17_bit7_index1(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
'''
if text.count(old) != 1:
    raise SystemExit(f"dispatch anchor expected once, found {text.count(old)}")
path.write_text(text.replace(old, new, 1))
