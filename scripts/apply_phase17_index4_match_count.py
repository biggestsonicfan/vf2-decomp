from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
marker = 'static vf2_status phase17_index4_render_descriptors(\n'
if marker not in text:
    raise SystemExit('index4 marker not found')
if 'finish_frame_phase17_index4_match_count' in text:
    raise SystemExit('match count recovery already present')

proto = '''static vf2_status compute_table_crc16(\n    const vf2_model2a *machine,\n    uint32_t source,\n    uint32_t count,\n    uint16_t *result\n);\n\n'''
text = text.replace(marker, proto + marker, 1)

execute_marker = 'static vf2_status execute_frame_phase17_bit7_index4(\n'
helper = r'''static vf2_status finish_frame_phase17_index4_match_count(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(3417)
        : (edit_delta < 0 ? UINT64_C(3415) : UINT64_C(3045));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(40);
    vf2_status status = VF2_OK;

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
    cpu->registers[9] = UINT32_C(0x00008800);
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01000318);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (edit_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
if execute_marker not in text:
    raise SystemExit('execute index4 marker not found')
text = text.replace(execute_marker, helper + execute_marker, 1)

old_decl = '''    int navigation_delta = 0;\n    uint64_t instructions = UINT64_C(3044);'''
new_decl = '''    int navigation_delta = 0;\n    int edit_delta = 0;\n    uint64_t instructions = UINT64_C(3044);'''
if old_decl not in text:
    raise SystemExit('index4 declarations not found')
text = text.replace(old_decl, new_decl, 1)

old_input = '''    if (navigation_flags == UINT32_C(0x1000)) {\n        navigation_delta = 1;\n        instructions = UINT64_C(3050);\n        calls = UINT64_C(37);\n    } else if (navigation_flags == UINT32_C(0x2000)) {\n        navigation_delta = -1;\n        instructions = UINT64_C(3048);\n        calls = UINT64_C(37);\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n\n    /* Editing/exit handlers are recovered separately.  An idle dispatch is\n     * currently accepted only for EXIT, whose handler observes no TEST input. */\n    if (navigation_delta == 0 && phase_a5 != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n'''
new_input = '''    if (phase_a5 == UINT8_C(1) &&\n        (navigation_flags == UINT32_C(0x100) ||\n         navigation_flags == UINT32_C(0x200))) {\n        edit_delta = navigation_flags == UINT32_C(0x100) ? 1 : -1;\n    } else if (navigation_flags == UINT32_C(0x1000)) {\n        navigation_delta = 1;\n        instructions = UINT64_C(3050);\n        calls = UINT64_C(37);\n    } else if (navigation_flags == UINT32_C(0x2000)) {\n        navigation_delta = -1;\n        instructions = UINT64_C(3048);\n        calls = UINT64_C(37);\n    } else if (navigation_flags != 0u) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n\n    if (navigation_delta == 0 && phase_a5 > UINT8_C(1)) {\n        return VF2_ERROR_UNSUPPORTED;\n    }\n'''
if old_input not in text:
    raise SystemExit('index4 input block not found')
text = text.replace(old_input, new_input, 1)

old_after_render = '''    if (status == VF2_OK && navigation_delta != 0) {\n'''
new_after_render = '''    if (status == VF2_OK && phase_a5 == UINT8_C(1) && edit_delta != 0) {\n        uint32_t base = 0u;\n        uint8_t value = 0u;\n        uint16_t crc = 0u;\n\n        status = vf2_model2a_read_u32(\n            machine, UINT32_C(0x0050016c), &base\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_read(\n                machine, base + UINT32_C(0x3340), &value, sizeof(value)\n            );\n        }\n        if (status == VF2_OK) {\n            int next = (int)value + edit_delta;\n            if (next < 2) {\n                next = 5;\n            } else if (next > 5) {\n                next = 2;\n            }\n            value = (uint8_t)next;\n            status = vf2_model2a_write(\n                machine, base + UINT32_C(0x3340), &value, sizeof(value)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write(\n                machine, UINT32_C(0x01d03340), &value, sizeof(value)\n            );\n        }\n        if (status == VF2_OK) {\n            status = compute_table_crc16(\n                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc\n            );\n        }\n        if (status == VF2_OK) {\n            status = write_u16(machine, UINT32_C(0x01d03302), crc);\n        }\n        if (status != VF2_OK) {\n            return status;\n        }\n\n        status = vf2_model2a_write(\n            machine, UINT32_C(0x0050002b), &selector_mirror,\n            sizeof(selector_mirror)\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_write(\n                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write_u32(\n                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write_u32(\n                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)\n            );\n        }\n        if (status != VF2_OK) {\n            return status;\n        }\n        return finish_frame_phase17_index4_match_count(\n            machine, cpu, report, edit_delta, crc, characters\n        );\n    }\n\n    if (status == VF2_OK && navigation_delta != 0) {\n'''
if old_after_render not in text:
    raise SystemExit('index4 post-render block not found')
text = text.replace(old_after_render, new_after_render, 1)

old_return = '''    return finish_frame_phase17_index4_observed(\n        machine, cpu, report, navigation_delta,\n        instructions, calls, characters\n    );\n}\n'''
new_return = '''    if (phase_a5 == UINT8_C(1)) {\n        return finish_frame_phase17_index4_match_count(\n            machine, cpu, report, 0, 0u, characters\n        );\n    }\n    return finish_frame_phase17_index4_observed(\n        machine, cpu, report, navigation_delta,\n        instructions, calls, characters\n    );\n}\n'''
if old_return not in text:
    raise SystemExit('index4 final return not found')
text = text.replace(old_return, new_return, 1)

path.write_text(text)
