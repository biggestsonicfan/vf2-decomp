from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
start = text.index("static vf2_status execute_frame_phase17_bit7_index3(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)
block = text[start:end]

old_guard = """        selector_mask != UINT32_C(0x00020000) || phase_a5 != 0u ||
        phase_a6 != UINT8_C(0xff)) {
"""
new_guard = """        selector_mask != UINT32_C(0x00020000) ||
        (phase_a5 != 0u && phase_a5 != UINT8_C(8)) ||
        phase_a6 != UINT8_C(0xff)) {
"""
if block.count(old_guard) != 1:
    raise SystemExit(f"guard anchor count={block.count(old_guard)}")
block = block.replace(old_guard, new_guard, 1)

anchor = """    /* 0x60600 builds four 9-row bands.  Each band contains sixteen
"""
transition = r'''    if (phase_a5 == UINT8_C(8)) {
        const uint8_t next_a5_value = UINT8_C(9);
        const uint8_t spill_value = UINT8_C(0x56);

        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &next_a5_value,
            sizeof(next_a5_value)
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x010016ac), UINT16_C(0x801c)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill_value,
                sizeof(spill_value)
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        cpu->executed_instructions += UINT64_C(87);
        cpu->procedure_calls += UINT64_C(4);
        cpu->procedure_returns += UINT64_C(4);
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
        cpu->registers[14] = UINT32_C(0x00000110);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x0000001c);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = 0u;
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
        report->changed_values = UINT64_C(3);
        report->bytes_written = 4u;
        report->recovered_instruction_count = UINT64_C(87);
        report->recovered_procedure_calls = UINT64_C(4);
        report->recovered_procedure_returns = UINT64_C(5);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

'''
if block.count(anchor) != 1:
    raise SystemExit(f"renderer anchor count={block.count(anchor)}")
block = block.replace(anchor, transition + anchor, 1)
text = text[:start] + block + text[end:]
path.write_text(text)
