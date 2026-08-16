from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

old_decl = """    uint32_t navigation_flags = 0u;
    uint32_t previous_flags = 0u;
"""
new_decl = """    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
"""
# Restrict replacement to index1 helper by locating after its function header.
start = text.index("static vf2_status execute_frame_phase17_bit7_index1(")
end = text.index("static vf2_status execute_frame_phase17_bit7_index11(", start)
block = text[start:end]
if block.count(old_decl) != 1:
    raise SystemExit(f"index1 declarations: found {block.count(old_decl)}")
block = block.replace(old_decl, new_decl, 1)

old_reads = """    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
"""
new_reads = """    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &released_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
"""
if block.count(old_reads) != 1:
    raise SystemExit(f"index1 read anchor: found {block.count(old_reads)}")
block = block.replace(old_reads, new_reads, 1)

old_guard = """        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 != 0u || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (index = 0u; status == VF2_OK &&
"""
new_guard = """        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        released_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);

        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += UINT64_C(1622);
        cpu->procedure_calls += UINT64_C(37);
        cpu->procedure_returns += UINT64_C(37);
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
        cpu->registers[14] = UINT32_C(0x0000010a);
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
        report->changed_values = UINT64_C(3);
        report->bytes_written = 3u;
        report->recovered_instruction_count = UINT64_C(1622);
        report->recovered_procedure_calls = UINT64_C(37);
        report->recovered_procedure_returns = UINT64_C(38);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    for (index = 0u; status == VF2_OK &&
"""
if block.count(old_guard) != 1:
    raise SystemExit(f"index1 guard: found {block.count(old_guard)}")
block = block.replace(old_guard, new_guard, 1)
text = text[:start] + block + text[end:]
path.write_text(text)
