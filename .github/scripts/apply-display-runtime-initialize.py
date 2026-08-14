from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"needle count for {path}: {text.count(old)}")
    p.write_text(text.replace(old, new, 1))

replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,\n    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,",
    "    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,\n    VF2_HYBRID_BRIDGE_DISPLAY_TRANSFORM_DEFAULTS,\n    VF2_HYBRID_BRIDGE_DISPLAY_RUNTIME_INITIALIZE,\n    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY UINT32_C(0x0004b410)\n#define VF2_PALETTE_PAGE_UPLOAD_ENTRY",
    "#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY UINT32_C(0x0004b410)\n#define VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY UINT32_C(0x0002eab8)\n#define VF2_DISPLAY_RUNTIME_INITIALIZE_CHILD_RETURN UINT32_C(0x0002ec20)\n#define VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY UINT32_C(0x00031004)\n#define VF2_PALETTE_PAGE_UPLOAD_ENTRY",
)
replace_once(
    "src/recovered/texture_bridge_internal.h",
    "vf2_status execute_video_command_submit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
    "vf2_status execute_video_command_submit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\nvf2_status execute_display_transform_defaults(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\nvf2_status execute_display_runtime_initialize(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);\n",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_VIDEO_COMMAND_SUBMIT_ENTRY:\n        status = execute_video_command_submit(machine, cpu, &local_report);\n        break;",
    "    case VF2_VIDEO_COMMAND_SUBMIT_ENTRY:\n        status = execute_video_command_submit(machine, cpu, &local_report);\n        break;\n    case VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY:\n        status = execute_display_transform_defaults(machine, cpu, &local_report);\n        break;\n    case VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY:\n        status = execute_display_runtime_initialize(machine, cpu, &local_report);\n        break;",
)

function = r'''
vf2_status execute_display_transform_defaults(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t state = 0u;
    vf2_status status = VF2_OK;
    static const uint32_t target[3] = {
        UINT32_C(0x40c00000), UINT32_C(0x40966666), UINT32_C(0x41940000)
    };
    static const uint32_t zero[3] = {0u, 0u, 0u};

    if (machine == NULL || cpu == NULL || report == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    if (cpu->local_frame_depth == 0u) return VF2_ERROR_UNSUPPORTED;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050084c), &state);
    if (status == VF2_OK) status = vf2_model2a_write(machine, state + UINT32_C(0x54), target, sizeof(target));
    if (status == VF2_OK) status = vf2_model2a_write(machine, state + UINT32_C(0x60), zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, state + UINT32_C(0x70), 0u);
    if (status != VF2_OK) return status;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(11));
    if (status != VF2_OK) return status;
    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_TRANSFORM_DEFAULTS;
    report->entry_address = VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(7);
    report->bytes_written = 28u;
    report->recovered_instruction_count = UINT64_C(11);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_display_runtime_initialize(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report child;
    uint32_t object = 0u;
    uint32_t state = 0u;
    uint8_t flags = 0u;
    uint8_t byte = 0u;
    vf2_status status = VF2_OK;
    static const struct { uint32_t off; uint32_t value; } words[] = {
        {0x234u, 0x3c872b02u}, {0x238u, 0x3ca3d70au},
        {0x264u, 0x409851ecu}, {0x268u, 0x40d051ecu},
        {0x240u, 0u}, {0x2bcu, 0u}, {0x2c0u, 0u},
        {0x2c4u, 0x3e19999au}, {0x2c8u, 0u}, {0x2ccu, 0xbcf5c28fu}
    };
    static const uint32_t half_zero[] = {0x260u,0x26cu,0x26eu,0x244u,0x2b0u,0x2b2u,0x2b4u,0x2b6u,0x2b8u,0x2bau};
    size_t i = 0u;

    if (machine == NULL || cpu == NULL || report == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    if (cpu->local_frame_depth == 0u) return VF2_ERROR_UNSUPPORTED;
    memset(&child, 0, sizeof(child));
    status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a160), UINT32_C(0xc0900000));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a164), UINT32_C(0x3dcccccd));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a168), UINT32_C(0x3dcccccd));
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0050a14d), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &object);
    if (status == VF2_OK) status = vf2_model2a_read(machine, object + UINT32_C(0xdf), &flags, 1u);
    flags = (uint8_t)(flags & UINT8_C(0xfe));
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0xdf), &flags, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27c), &byte, 1u);
    for (i = 0u; status == VF2_OK && i < sizeof(words)/sizeof(words[0]); ++i)
        status = vf2_model2a_write_u32(machine, object + words[i].off, words[i].value);
    for (i = 0u; status == VF2_OK && i < sizeof(half_zero)/sizeof(half_zero[0]); ++i)
        status = write_u16(machine, object + half_zero[i], 0u);
    if (status == VF2_OK) status = write_u16(machine, object + UINT32_C(0x23c), UINT16_C(13));
    byte = UINT8_C(88);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x23e), &byte, 1u);
    byte = 0u;
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27d), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x23f), &byte, 1u);
    byte = UINT8_C(0xff);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x246), &byte, 1u);
    byte = 0u;
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27e), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27f), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050084c), &state);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, state + UINT32_C(0x40), 0u);
    if (status != VF2_OK) return status;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY, VF2_DISPLAY_RUNTIME_INITIALIZE_CHILD_RETURN);
    if (status != VF2_OK) return status;
    cpu->executed_instructions += UINT64_C(72);
    status = execute_display_transform_defaults(machine, cpu, &child);
    if (status != VF2_OK) return status;
    if (cpu->ip != VF2_DISPLAY_RUNTIME_INITIALIZE_CHILD_RETURN) return VF2_ERROR_UNSUPPORTED;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(1));
    if (status != VF2_OK) return status;

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_RUNTIME_INITIALIZE;
    report->entry_address = VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(35) + child.changed_values;
    report->bytes_written = 87u + child.bytes_written;
    report->recovered_instruction_count = UINT64_C(73) + child.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = child.recovered_procedure_returns + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

'''
p = Path("src/recovered/texture_bridge_video.c")
text = p.read_text()
needle = "vf2_status execute_video_command_submit(\n"
if text.count(needle) != 1: raise SystemExit("video insertion needle mismatch")
p.write_text(text.replace(needle, function + needle, 1))

Path("decomp/i960/notes/display_runtime_initialize_executable_v0026.md").write_text('''# Executable display runtime initializer (v0.0.26)\n\n`0x0002eab8..0x0002ec20` is recovered as an executable caller of `display_transform_defaults` at `0x00031004`. An isolated ROM differential with valid work-RAM objects at `0x00500814` and `0x0050084c` executes 84 total instructions, 73 exclusive to the parent and 11 in the child, with one nested call and two returns.\n\nThe parent initializes the global display tuning words at `0x0050a160..0x0050a168`, normalizes the object pointed to by `0x00500814` across offsets `0xdf` and `0x234..0x2cc`, clears flags at `(*(u32*)0x0050084c)+0x40`, and calls the child. The child writes target transform `(6.0f, 4.7f, 18.5f)` at `+0x54`, zeroes the secondary triple at `+0x60`, and clears `+0x70`.\n''')
