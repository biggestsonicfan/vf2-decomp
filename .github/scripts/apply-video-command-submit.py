from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"needle count for {path}: {text.count(old)}")
    p.write_text(text.replace(old, new, 1))


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS,\n    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,",
    "    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS,\n    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,\n    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN UINT32_C(0x0001ff10)\n#define VF2_PALETTE_PAGE_UPLOAD_ENTRY",
    "#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN UINT32_C(0x0001ff10)\n#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY UINT32_C(0x0004b410)\n#define VF2_PALETTE_PAGE_UPLOAD_ENTRY",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "vf2_status execute_display_profile_mode_constants(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
    "vf2_status execute_display_profile_mode_constants(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\nvf2_status execute_video_command_submit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY:\n        status = execute_display_profile_mode_constants(machine, cpu, &local_report);\n        break;",
    "    case VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY:\n        status = execute_display_profile_mode_constants(machine, cpu, &local_report);\n        break;\n    case VF2_VIDEO_COMMAND_SUBMIT_ENTRY:\n        status = execute_video_command_submit(machine, cpu, &local_report);\n        break;",
)

function = r'''
vf2_status execute_video_command_submit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;
    const uint32_t packet = UINT32_C(0x005502e0);

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(machine, UINT32_C(0x00550000), UINT32_C(1));
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, packet, UINT32_C(3));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, packet + UINT32_C(4),
            cpu->registers[VF2_I960_G0_REGISTER]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, packet + UINT32_C(8),
            cpu->registers[VF2_I960_G0_REGISTER + 1u]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, packet + UINT32_C(12),
            cpu->registers[VF2_I960_G0_REGISTER + 2u]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = packet;
    cpu->registers[15] = UINT32_C(3);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(9));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT;
    report->entry_address = VF2_VIDEO_COMMAND_SUBMIT_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(5);
    report->bytes_written = 20u;
    report->recovered_instruction_count = UINT64_C(9);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
p = Path("src/recovered/texture_bridge_video.c")
text = p.read_text()
needle = "vf2_status execute_display_profile_mode_constants(\n"
if text.count(needle) != 1:
    raise SystemExit(f"video insertion needle count: {text.count(needle)}")
p.write_text(text.replace(needle, function + needle, 1))

Path("decomp/i960/notes/video_command_submit_executable_v0026.md").write_text(r'''# Executable video command submit (v0.0.26)

`0x0004b410..0x0004b448` is now recovered as a standalone executable leaf. It writes the command-ready word at `0x00550000`, emits selector `3` at `0x005502e0`, and copies the caller's `g0/g1/g2` into the three packet arguments at `+4/+8/+12`.

The leaf executes exactly 9 instructions including its final `ret`, has no nested calls, writes 20 bytes, leaves `r3 = 0x005502e0` and `r15 = 3`, and otherwise preserves the caller's global argument registers.
''')
