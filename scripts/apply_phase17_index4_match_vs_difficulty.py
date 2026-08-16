from pathlib import Path

path = Path('src/recovered/texture_bridge_match.c')
text = path.read_text()
if 'finish_frame_phase17_index4_difficulty' in text:
    raise SystemExit('difficulty recovery already present')

# Generalize the existing MATCH COUNT poststate over states 1 and 2.
old_sig = '''static vf2_status finish_frame_phase17_index4_match_count(\n    vf2_model2a *machine,\n    vf2_i960_cpu *cpu,\n    vf2_hybrid_bridge_report *report,\n    int edit_delta,\n    uint16_t crc,\n    uint64_t characters\n)'''
new_sig = '''static vf2_status finish_frame_phase17_index4_match_count(\n    vf2_model2a *machine,\n    vf2_i960_cpu *cpu,\n    vf2_hybrid_bridge_report *report,\n    uint8_t selection,\n    int edit_delta,\n    uint16_t crc,\n    uint64_t characters\n)'''
if old_sig not in text:
    raise SystemExit('match-count finish signature not found')
text = text.replace(old_sig, new_sig, 1)
text = text.replace(
    '    cpu->registers[25] = UINT32_C(0x01000318);\n',
    '    cpu->registers[25] = UINT32_C(0x01000318) +\n        ((uint32_t)selection - UINT32_C(1)) * UINT32_C(0x100);\n',
    1
)

execute_marker = 'static vf2_status execute_frame_phase17_bit7_index4(\n'
helper = r'''static vf2_status finish_frame_phase17_index4_difficulty(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(3430)
        : (edit_delta < 0 ? UINT64_C(3427) : UINT64_C(3045));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(41);
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
    cpu->registers[25] = UINT32_C(0x01000598);
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
    raise SystemExit('index4 execute marker not found')
text = text.replace(execute_marker, helper + execute_marker, 1)

# Expand edit input handling from state 1 to states 1..3.
old = '''    if (phase_a5 == UINT8_C(1) &&\n        (navigation_flags == UINT32_C(0x100) ||\n         navigation_flags == UINT32_C(0x200))) {'''
new = '''    if (phase_a5 >= UINT8_C(1) && phase_a5 <= UINT8_C(3) &&\n        (navigation_flags == UINT32_C(0x100) ||\n         navigation_flags == UINT32_C(0x200))) {'''
if old not in text:
    raise SystemExit('edit input condition not found')
text = text.replace(old, new, 1)
text = text.replace(
    '    if (navigation_delta == 0 && phase_a5 > UINT8_C(1)) {\n',
    '    if (navigation_delta == 0 && phase_a5 > UINT8_C(3)) {\n',
    1
)

# Generalize MATCH COUNT edit body to state 1 or 2 and insert DIFFICULTY body.
old_match = '''    if (status == VF2_OK && phase_a5 == UINT8_C(1) && edit_delta != 0) {\n        uint32_t base = 0u;\n        uint8_t value = 0u;\n        uint16_t crc = 0u;\n\n        status = vf2_model2a_read_u32(\n            machine, UINT32_C(0x0050016c), &base\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_read(\n                machine, base + UINT32_C(0x3340), &value, sizeof(value)\n            );\n        }'''
new_match = '''    if (status == VF2_OK &&\n        (phase_a5 == UINT8_C(1) || phase_a5 == UINT8_C(2)) &&\n        edit_delta != 0) {\n        const uint32_t value_offset = UINT32_C(0x3340) +\n            ((uint32_t)phase_a5 - UINT32_C(1));\n        uint32_t base = 0u;\n        uint8_t value = 0u;\n        uint16_t crc = 0u;\n\n        status = vf2_model2a_read_u32(\n            machine, UINT32_C(0x0050016c), &base\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_read(\n                machine, base + value_offset, &value, sizeof(value)\n            );\n        }'''
if old_match not in text:
    raise SystemExit('match edit body start not found')
text = text.replace(old_match, new_match, 1)
text = text.replace(
    '                machine, base + UINT32_C(0x3340), &value, sizeof(value)\n',
    '                machine, base + value_offset, &value, sizeof(value)\n',
    1
)
text = text.replace(
    '                machine, UINT32_C(0x01d03340), &value, sizeof(value)\n',
    '                machine, UINT32_C(0x01d00000) + value_offset,\n                &value, sizeof(value)\n',
    1
)
text = text.replace(
    '            machine, cpu, report, edit_delta, crc, characters\n        );\n    }\n\n    if (status == VF2_OK && navigation_delta != 0) {',
    '            machine, cpu, report, phase_a5, edit_delta, crc, characters\n        );\n    }\n\n    if (status == VF2_OK && phase_a5 == UINT8_C(3) && edit_delta != 0) {\n        uint32_t base = 0u;\n        uint8_t value = 0u;\n        uint8_t width = 0u;\n        uint16_t energy_1p = 0u;\n        uint16_t energy_vs = 0u;\n        uint16_t crc = 0u;\n\n        status = vf2_model2a_read_u32(\n            machine, UINT32_C(0x0050016c), &base\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_read(\n                machine, base + UINT32_C(0x3342), &value, sizeof(value)\n            );\n        }\n        if (status == VF2_OK) {\n            int next = (int)value + edit_delta;\n            if (next < 0) {\n                next = 3;\n            } else if (next > 3) {\n                next = 0;\n            }\n            value = (uint8_t)next;\n            status = vf2_model2a_write(\n                machine, base + UINT32_C(0x3342), &value, sizeof(value)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write(\n                machine, UINT32_C(0x01d03342), &value, sizeof(value)\n            );\n        }\n        if (status == VF2_OK) {\n            status = read_u16(\n                machine, UINT32_C(0x0005b32c) + (uint32_t)value * 2u,\n                &energy_1p\n            );\n        }\n        if (status == VF2_OK) {\n            status = read_u16(\n                machine, UINT32_C(0x0005b334) + (uint32_t)value * 2u,\n                &energy_vs\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_read(\n                machine, UINT32_C(0x0005b33c) + (uint32_t)value,\n                &width, sizeof(width)\n            );\n        }\n        if (status == VF2_OK) {\n            status = write_u16(machine, base + UINT32_C(0x3352), energy_1p);\n        }\n        if (status == VF2_OK) {\n            status = write_u16(machine, UINT32_C(0x01d03352), energy_1p);\n        }\n        if (status == VF2_OK) {\n            status = write_u16(machine, base + UINT32_C(0x3354), energy_vs);\n        }\n        if (status == VF2_OK) {\n            status = write_u16(machine, UINT32_C(0x01d03354), energy_vs);\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write(\n                machine, base + UINT32_C(0x334f), &width, sizeof(width)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write(\n                machine, UINT32_C(0x01d0334f), &width, sizeof(width)\n            );\n        }\n        if (status == VF2_OK) {\n            status = compute_table_crc16(\n                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc\n            );\n        }\n        if (status == VF2_OK) {\n            status = write_u16(machine, UINT32_C(0x01d03302), crc);\n        }\n        if (status != VF2_OK) {\n            return status;\n        }\n        status = vf2_model2a_write(\n            machine, UINT32_C(0x0050002b), &selector_mirror,\n            sizeof(selector_mirror)\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_write(\n                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write_u32(\n                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)\n            );\n        }\n        if (status == VF2_OK) {\n            status = vf2_model2a_write_u32(\n                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)\n            );\n        }\n        if (status != VF2_OK) {\n            return status;\n        }\n        return finish_frame_phase17_index4_difficulty(\n            machine, cpu, report, edit_delta, crc, characters\n        );\n    }\n\n    if (status == VF2_OK && navigation_delta != 0) {',
    1
)

old_final = '''    if (phase_a5 == UINT8_C(1)) {\n        return finish_frame_phase17_index4_match_count(\n            machine, cpu, report, 0, 0u, characters\n        );\n    }'''
new_final = '''    if (phase_a5 == UINT8_C(1) || phase_a5 == UINT8_C(2)) {\n        return finish_frame_phase17_index4_match_count(\n            machine, cpu, report, phase_a5, 0, 0u, characters\n        );\n    }\n    if (phase_a5 == UINT8_C(3)) {\n        return finish_frame_phase17_index4_difficulty(\n            machine, cpu, report, 0, 0u, characters\n        );\n    }'''
if old_final not in text:
    raise SystemExit('index4 final item1 branch not found')
text = text.replace(old_final, new_final, 1)

path.write_text(text)
