from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"needle count for {path}: {text.count(old)}")
    p.write_text(text.replace(old, new, 1))


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_UNIT_FILL,\n    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,",
    "    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_UNIT_FILL,\n    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS,\n    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "#define VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY UINT32_C(0x0001fee4)\n#define VF2_PALETTE_PAGE_UPLOAD_ENTRY",
    "#define VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY UINT32_C(0x0001fee4)\n#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY UINT32_C(0x0001ff0c)\n#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN UINT32_C(0x0001ff10)\n#define VF2_PALETTE_PAGE_UPLOAD_ENTRY",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "vf2_status execute_display_profile_unit_fill(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
    "vf2_status execute_display_profile_unit_fill(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\nvf2_status execute_display_profile_mode_constants(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY:\n        status = execute_display_profile_unit_fill(machine, cpu, &local_report);\n        break;",
    "    case VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY:\n        status = execute_display_profile_unit_fill(machine, cpu, &local_report);\n        break;\n    case VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY:\n        status = execute_display_profile_mode_constants(machine, cpu, &local_report);\n        break;",
)

function = r'''
vf2_status execute_display_profile_mode_constants(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report child_report;
    uint8_t mode = 0u;
    uint64_t exclusive_instructions = UINT64_C(5);
    uint64_t changed_values = UINT64_C(26);
    size_t bytes_written = 26u * 4u;
    vf2_status status = VF2_OK;

    static const uint32_t mode6_addresses[11] = {
        UINT32_C(0x0050a0e4), UINT32_C(0x0050a0e8),
        UINT32_C(0x0050a0f0), UINT32_C(0x0050a0f8),
        UINT32_C(0x0050a100), UINT32_C(0x0050a118),
        UINT32_C(0x0050a11c), UINT32_C(0x0050a124),
        UINT32_C(0x0050a128), UINT32_C(0x0050a12c),
        UINT32_C(0x0050a134)
    };
    static const uint32_t mode6_values[11] = {
        UINT32_C(0x3f0a3d71), UINT32_C(0x3f0a3d71),
        UINT32_C(0x3f6b851f), UINT32_C(0x3f5eb852),
        UINT32_C(0x3f028f5c), UINT32_C(0x3f0a3d71),
        UINT32_C(0x3f0a3d71), UINT32_C(0x3f07ae14),
        UINT32_C(0x3f28f5c3), UINT32_C(0x3f11eb85),
        UINT32_C(0x3f028f5c)
    };

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&child_report, 0, sizeof(child_report));

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY,
        VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(1);
    status = execute_display_profile_unit_fill(machine, cpu, &child_report);
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &mode, sizeof(mode)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = (uint32_t)mode;

    if (mode == UINT8_C(10)) {
        exclusive_instructions = UINT64_C(8);
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a124), UINT32_C(0x3f0f5c29)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x0050a128), UINT32_C(0x3ef0a3d7)
            );
        }
        cpu->registers[15] = UINT32_C(0x3ef0a3d7);
        set_equal_condition(cpu);
        changed_values += UINT64_C(2);
        bytes_written += 8u;
    } else if (mode == UINT8_C(6)) {
        size_t index = 0u;
        exclusive_instructions = UINT64_C(27);
        for (index = 0u; status == VF2_OK && index < 11u; ++index) {
            status = vf2_model2a_write_u32(
                machine, mode6_addresses[index], mode6_values[index]
            );
        }
        cpu->registers[15] = UINT32_C(0x3f028f5c);
        set_equal_condition(cpu);
        changed_values += UINT64_C(11);
        bytes_written += 44u;
    } else {
        set_signed_condition(cpu, INT32_C(6), (int32_t)mode);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += exclusive_instructions - UINT64_C(2);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(1));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS;
    report->entry_address = VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count =
        exclusive_instructions + child_report.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns =
        child_report.recovered_procedure_returns + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
p = Path("src/recovered/texture_bridge_video.c")
text = p.read_text()
needle = "vf2_status execute_display_profile_unit_fill(\n"
if text.count(needle) != 1:
    raise SystemExit(f"video insertion needle count: {text.count(needle)}")
p.write_text(text.replace(needle, function + needle, 1))

Path("decomp/i960/notes/display_profile_mode_constants_executable_v0026.md").write_text(r'''# Executable display profile mode constants (v0.0.26)

`0x0001ff0c..0x0001fff8` is recovered as an executable caller of `display_profile_unit_fill` (`0x0001fee4`). The child uses the architectural i960 call frame and returns to `0x0001ff10` before mode dispatch.

Exact exclusive instruction counts from ROM disassembly are 5 instructions for the default branch, 8 for mode 10, and 27 for mode 6. These totals include the call instruction and parent `ret`, but exclude the child's independently recovered 108 instructions.

Mode 10 writes two tuning words. Mode 6 writes eleven tuning words. Other modes return immediately after the unit fill. The recovery also preserves the final `r15` value and comparison condition produced by the executed `cmpobe` path.
''')
