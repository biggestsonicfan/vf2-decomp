from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"needle count for {path}: {text.count(old)}")
    p.write_text(text.replace(old, new, 1))


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS,\n    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,",
    "    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS,\n    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_APPLY,\n    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN UINT32_C(0x0001ff10)\n#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY",
    "#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN UINT32_C(0x0001ff10)\n#define VF2_DISPLAY_PROFILE_APPLY_ENTRY UINT32_C(0x0001fcc0)\n#define VF2_DISPLAY_PROFILE_APPLY_MODE_RETURN UINT32_C(0x0001fdd4)\n#define VF2_DISPLAY_PROFILE_APPLY_COLOR_RETURN UINT32_C(0x0001fe64)\n#define VF2_DISPLAY_PROFILE_APPLY_COMMAND_RETURN UINT32_C(0x0001fe78)\n#define VF2_DISPLAY_PROFILE_APPLY_RUNTIME_RETURN UINT32_C(0x0001fedc)\n#define VF2_DISPLAY_PROFILE_APPLY_TABLE_RETURN UINT32_C(0x0001fee0)\n#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "vf2_status execute_display_profile_mode_constants(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
    "vf2_status execute_display_profile_mode_constants(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\nvf2_status execute_display_profile_apply(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY:\n        status = execute_display_profile_mode_constants(machine, cpu, &local_report);\n        break;",
    "    case VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY:\n        status = execute_display_profile_mode_constants(machine, cpu, &local_report);\n        break;\n    case VF2_DISPLAY_PROFILE_APPLY_ENTRY:\n        status = execute_display_profile_apply(machine, cpu, &local_report);\n        break;",
)

function = r'''
vf2_status execute_display_profile_apply(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report mode_report;
    vf2_hybrid_bridge_report color_report;
    vf2_hybrid_bridge_report command_report;
    vf2_hybrid_bridge_report runtime_report;
    vf2_hybrid_bridge_report table_report;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t flags = 0u;
    uint32_t table = 0u;
    uint32_t value32 = 0u;
    uint16_t value16 = 0u;
    uint8_t state0 = 0u;
    uint8_t state1 = 0u;
    uint8_t mode = 0u;
    uint8_t gate = 0u;
    uint8_t value8 = 0u;
    uint64_t exclusive = 0u;
    uint64_t child_instructions = 0u;
    uint64_t child_calls = 0u;
    uint64_t child_returns = 0u;
    uint64_t changed_values = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&mode_report, 0, sizeof(mode_report));
    memset(&color_report, 0, sizeof(color_report));
    memset(&command_report, 0, sizeof(command_report));
    memset(&runtime_report, 0, sizeof(runtime_report));
    memset(&table_report, 0, sizeof(table_report));

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &player1); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_read(machine, player0 + UINT32_C(0x1b1), &state0, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[3] = player0;
    cpu->registers[4] = player1;
    cpu->registers[14] = state0;

    ++exclusive; /* cmpobne 2,state0 */
    if (state0 == UINT8_C(2)) {
        status = vf2_model2a_read(machine, player1 + UINT32_C(0x1b1), &state1, 1u); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[14] = state1;
        ++exclusive; /* cmpobe 1,state1 */
        if (state1 == UINT8_C(1)) {
            mode = UINT8_C(12); cpu->registers[15] = 12u; ++exclusive;
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
            if (status != VF2_OK) return status;
            flags &= ~(UINT32_C(1) << 20u); cpu->registers[15] = flags; ++exclusive;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
            if (status != VF2_OK) return status;
            changed_values += UINT64_C(2); bytes_written += 5u;
            goto profile_flags;
        }
    }

    status = vf2_model2a_read(machine, player0 + UINT32_C(0x1b1), &state0, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[14] = state0;
    ++exclusive; /* cmpobne 1,state0 */
    if (state0 == UINT8_C(1)) {
        status = vf2_model2a_read(machine, player1 + UINT32_C(0x1b1), &state1, 1u); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[14] = state1;
        ++exclusive; /* cmpobne 2,state1 */
        if (state1 == UINT8_C(2)) {
            mode = UINT8_C(12); cpu->registers[15] = 12u; ++exclusive;
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
            if (status != VF2_OK) return status;
            flags &= ~(UINT32_C(1) << 20u); cpu->registers[15] = flags; ++exclusive;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
            if (status != VF2_OK) return status;
            changed_values += UINT64_C(2); bytes_written += 5u;
        }
    }

profile_flags:
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = flags;
    ++exclusive; /* bbc 21 */
    if ((flags & (UINT32_C(1) << 21u)) != 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[15] = flags;
        ++exclusive; /* bbs 20 */
        if ((flags & (UINT32_C(1) << 20u)) != 0u) {
            goto force_mode10;
        }
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = mode;
    ++exclusive; /* cmpobne 10 */
    if (mode == UINT8_C(10)) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050004c), &gate, 1u); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[14] = gate;
        ++exclusive; /* cmpobe 2 */
        if (gate == UINT8_C(2)) {
            mode = UINT8_C(11); cpu->registers[15] = 11u; ++exclusive;
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
            if (status != VF2_OK) return status;
            changed_values += UINT64_C(1); bytes_written += 1u;
            goto normal_constants;
        }
force_mode10:
        mode = UINT8_C(10); cpu->registers[15] = 10u; ++exclusive;
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
        if (status != VF2_OK) return status;
        flags |= UINT32_C(1) << 20u; cpu->registers[15] = flags; ++exclusive;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[15] = UINT32_C(0x3a3117c4); ++exclusive;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a000), UINT32_C(0x3a3117c4)); ++exclusive;
        if (status == VF2_OK) { cpu->registers[15] = UINT32_C(0x40000000); ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a004), UINT32_C(0x40000000)); ++exclusive; }
        if (status != VF2_OK) return status;
        ++exclusive; /* b 0x1fdd0 */
        changed_values += UINT64_C(4); bytes_written += 13u;
        goto call_mode_constants;
    }

normal_constants:
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
    if (status != VF2_OK) return status;
    flags &= ~(UINT32_C(1) << 20u); cpu->registers[15] = flags; ++exclusive;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = UINT32_C(0x3b32674f); ++exclusive;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a000), UINT32_C(0x3b32674f)); ++exclusive;
    if (status == VF2_OK) { cpu->registers[15] = UINT32_C(0x3f800000); ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a004), UINT32_C(0x3f800000)); ++exclusive; }
    if (status != VF2_OK) return status;
    changed_values += UINT64_C(3); bytes_written += 12u;

call_mode_constants:
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY, VF2_DISPLAY_PROFILE_APPLY_MODE_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_display_profile_mode_constants(machine, cpu, &mode_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_MODE_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    cpu->registers[15] = UINT32_C(0x1388); ++exclusive;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00501018), UINT32_C(0x1388)); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[12] = mode;
    cpu->registers[4] = (uint32_t)mode << 8u; ++exclusive;
    table = UINT32_C(0x0006ee00) + cpu->registers[4];
    status = vf2_model2a_read(machine, table + UINT32_C(0xae), &value8, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = value8;
    ++exclusive; /* cmpobne 4 */
    if (value8 == UINT8_C(4)) {
        cpu->registers[15] = UINT32_C(0x10cc); ++exclusive;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00501018), UINT32_C(0x10cc)); ++exclusive;
        if (status != VF2_OK) return status;
        changed_values += UINT64_C(1); bytes_written += 4u;
    }
    cpu->registers[6] = UINT32_C(0x018021ee); ++exclusive;
    status = read_u16(machine, UINT32_C(0x0006ef0c) + cpu->registers[4], &value16); ++exclusive;
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x018021ee), value16); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_read(machine, table + UINT32_C(0xae), &value8, 1u); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00500170), &value8, 1u); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, table + UINT32_C(0xa4), &value32); ++exclusive;
    if (status == VF2_OK) status = read_u16(machine, table + UINT32_C(0xa8), &value16); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[5] = value32;
    cpu->registers[6] = value16;
    { uint16_t value16b = 0u;
      status = read_u16(machine, table + UINT32_C(0xaa), &value16b); ++exclusive;
      if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00501098), value32); ++exclusive;
      if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x00501020), value16); ++exclusive;
      if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x00501022), value16b); ++exclusive;
      if (status != VF2_OK) return status;
      cpu->registers[7] = value16b;
    }
    changed_values += UINT64_C(5); bytes_written += 13u;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_COLOR_PROFILE_APPLY_ENTRY, VF2_DISPLAY_PROFILE_APPLY_COLOR_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_display_color_profile_apply(machine, cpu, &color_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_COLOR_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    status = vf2_model2a_read_u32(machine, table + UINT32_C(0xb0), &cpu->registers[VF2_I960_G0_REGISTER]); ++exclusive;
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, table + UINT32_C(0xb4), &cpu->registers[VF2_I960_G0_REGISTER + 1u]); ++exclusive;
    if (status != VF2_OK) return status;
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_VIDEO_COMMAND_SUBMIT_ENTRY, VF2_DISPLAY_PROFILE_APPLY_COMMAND_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_video_command_submit(machine, cpu, &command_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_COMMAND_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    cpu->registers[15] = 0u; ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a014), 0u); ++exclusive;
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a018), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a01c), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a020), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a022), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a024), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a026), 0u); ++exclusive; }
    if (status != VF2_OK) return status;
    changed_values += UINT64_C(7); bytes_written += 20u;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY, VF2_DISPLAY_PROFILE_APPLY_RUNTIME_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_display_runtime_initialize(machine, cpu, &runtime_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_RUNTIME_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_VIDEO_TABLE_EXPAND_128_ENTRY, VF2_DISPLAY_PROFILE_APPLY_TABLE_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_video_table_expand_128(machine, cpu, &table_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_TABLE_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    ++exclusive; /* parent ret */
    cpu->executed_instructions += exclusive;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) return status;

    child_instructions = mode_report.recovered_instruction_count + color_report.recovered_instruction_count + command_report.recovered_instruction_count + runtime_report.recovered_instruction_count + table_report.recovered_instruction_count;
    child_calls = mode_report.recovered_procedure_calls + color_report.recovered_procedure_calls + command_report.recovered_procedure_calls + runtime_report.recovered_procedure_calls + table_report.recovered_procedure_calls;
    child_returns = mode_report.recovered_procedure_returns + color_report.recovered_procedure_returns + command_report.recovered_procedure_returns + runtime_report.recovered_procedure_returns + table_report.recovered_procedure_returns;
    changed_values += mode_report.changed_values + color_report.changed_values + command_report.changed_values + runtime_report.changed_values + table_report.changed_values;
    bytes_written += mode_report.bytes_written + color_report.bytes_written + command_report.bytes_written + runtime_report.bytes_written + table_report.bytes_written;

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_APPLY;
    report->entry_address = VF2_DISPLAY_PROFILE_APPLY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = exclusive + child_instructions;
    report->recovered_procedure_calls = UINT64_C(5) + child_calls;
    report->recovered_procedure_returns = UINT64_C(1) + child_returns;
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

Path("decomp/i960/notes/display_profile_apply_executable_v0026.md").write_text(r'''# Executable display profile apply (v0.0.26)

`0x0001fcc0..0x0001fee0` is recovered as the generic parent procedure rather than a selector-3-only snapshot. The implementation reproduces both player-state branches that can force mode 12, runtime flag bits 20/21, the mode 10/11 transitions, profile constants, table-derived video parameters, and the five direct calls to the already recovered children at `0x1ff0c`, `0x1fffc`, `0x4b410`, `0x2eab8`, and `0x11704`.

Parent-exclusive instruction accounting is accumulated instruction-by-instruction along the actual branch path. Child accounting remains compositional. The selector-3 synthetic `123638 / 15 / 15` delta is deliberately not reduced in this commit; that subtraction is reserved for the next differential checkpoint of the complete parent against the controlled selector-3 state.
''')
