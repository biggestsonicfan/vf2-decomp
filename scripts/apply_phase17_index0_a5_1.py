from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

old_guard = """        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 != 0u || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = write_phase17_index0_text(
"""
new_guard = """        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);

        /* 0x59164 uses a5 as a secondary dispatch selector.  The observed
         * second visit selects 0x59358, where 0x00500704 & 0x04000104 is
         * zero and the diagnostic wrapper immediately unwinds. */
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += UINT64_C(36);
        cpu->procedure_calls += UINT64_C(2);
        cpu->procedure_returns += UINT64_C(2);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(3);
        report->bytes_written = 3u;
        report->recovered_instruction_count = UINT64_C(36);
        report->recovered_procedure_calls = UINT64_C(2);
        report->recovered_procedure_returns = UINT64_C(3);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = write_phase17_index0_text(
"""
if text.count(old_guard) != 1:
    raise SystemExit(f"expected one guard anchor, found {text.count(old_guard)}")
path.write_text(text.replace(old_guard, new_guard, 1))
