from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"needle count for {path}: {text.count(old)}")
    p.write_text(text.replace(old, new, 1))


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,\n    VF2_HYBRID_BRIDGE_DISPLAY_RUNTIME_INITIALIZE,",
    "    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,\n    VF2_HYBRID_BRIDGE_VIDEO_TABLE_EXPAND_128,\n    VF2_HYBRID_BRIDGE_DISPLAY_RUNTIME_INITIALIZE,",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY UINT32_C(0x0004b410)\n#define VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY",
    "#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY UINT32_C(0x0004b410)\n#define VF2_VIDEO_TABLE_EXPAND_128_ENTRY UINT32_C(0x00011704)\n#define VF2_VIDEO_TABLE_EXPAND_128_COUNT UINT32_C(0x00078d0c)\n#define VF2_VIDEO_TABLE_EXPAND_128_SOURCE UINT32_C(0x00078d10)\n#define VF2_VIDEO_TABLE_EXPAND_128_DESTINATION UINT32_C(0x12800000)\n#define VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "vf2_status execute_video_command_submit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
    "vf2_status execute_video_command_submit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\nvf2_status execute_video_table_expand_128(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_VIDEO_COMMAND_SUBMIT_ENTRY:\n        status = execute_video_command_submit(machine, cpu, &local_report);\n        break;",
    "    case VF2_VIDEO_COMMAND_SUBMIT_ENTRY:\n        status = execute_video_command_submit(machine, cpu, &local_report);\n        break;\n    case VF2_VIDEO_TABLE_EXPAND_128_ENTRY:\n        status = execute_video_table_expand_128(machine, cpu, &local_report);\n        break;",
)

function = r'''
vf2_status execute_video_table_expand_128(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t encoded_count = 0u;
    uint32_t outer_iterations = 0u;
    uint32_t source = VF2_VIDEO_TABLE_EXPAND_128_SOURCE;
    uint32_t destination = VF2_VIDEO_TABLE_EXPAND_128_DESTINATION;
    uint32_t outer = 0u;
    uint8_t last_value = 0u;
    uint64_t instruction_count = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_VIDEO_TABLE_EXPAND_128_COUNT, &encoded_count
    );
    if (status != VF2_OK) {
        return status;
    }

    outer_iterations = encoded_count == 0u ? 1u : encoded_count;
    for (outer = 0u; outer < outer_iterations; ++outer) {
        uint32_t column = 0u;
        for (column = 0u; column < UINT32_C(128); ++column) {
            status = vf2_model2a_read(machine, source, &last_value, sizeof(last_value));
            if (status != VF2_OK) {
                return status;
            }
            status = vf2_model2a_write_u32(
                machine, destination, (uint32_t)last_value
            );
            if (status != VF2_OK) {
                return status;
            }
            ++source;
            destination += UINT32_C(4);
        }
    }

    cpu->registers[VF2_I960_G0_REGISTER] = destination;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = source;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] =
        encoded_count == 0u ? UINT32_MAX : 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
    cpu->registers[3] = (uint32_t)last_value;
    if (encoded_count == 0u) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
    } else {
        set_equal_condition(cpu);
    }

    instruction_count = UINT64_C(4) + UINT64_C(771) * outer_iterations;
    status = finish_recovered_procedure(machine, cpu, instruction_count);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_TABLE_EXPAND_128;
    report->entry_address = VF2_VIDEO_TABLE_EXPAND_128_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = (uint64_t)outer_iterations * UINT64_C(128);
    report->rows = outer_iterations;
    report->changed_values = (uint64_t)outer_iterations * UINT64_C(128);
    report->bytes_written = (size_t)outer_iterations * 128u * 4u;
    report->recovered_instruction_count = instruction_count;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
p = Path("src/recovered/texture_bridge_video.c")
text = p.read_text()
needle = "vf2_status execute_video_command_submit(\n"
if text.count(needle) != 1:
    raise SystemExit(f"video insertion needle count: {text.count(needle)}")
p.write_text(text.replace(needle, function + needle, 1))
