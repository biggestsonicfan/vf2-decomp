from pathlib import Path

WORKFLOW = ".github/workflows/integrate-v0024-active-prepare.yml"
SCRIPT = "tools/apply_v0024_active_prepare.py"


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END,\n",
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END,\n",
    "active-prepare enum",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "#define VF2_TEXTURE_STATUS_DISPATCH_RETURN UINT32_C(0x0004bd5c)\n",
    "#define VF2_TEXTURE_STATUS_DISPATCH_RETURN UINT32_C(0x0004bd5c)\n"
    "#define VF2_TEXTURE_ACTIVE_PREPARE_ENTRY UINT32_C(0x0004bde0)\n"
    "#define VF2_TEXTURE_ACTIVE_PREPARE_TARGET UINT32_C(0x0004d16c)\n"
    "#define VF2_TEXTURE_ACTIVE_PREPARE_RETURN UINT32_C(0x0004be6c)\n"
    "#define VF2_TEXTURE_ACTIVE_FLAGS UINT32_C(0x0055c2f4)\n"
    "#define VF2_TEXTURE_COORD_TABLE UINT32_C(0x0004c120)\n",
    "active-prepare constants",
)

active_prepare_function = r'''static vf2_status execute_texture_active_prepare_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t raw_count = 0u;
    uint16_t raw_x = 0u;
    uint16_t raw_y = 0u;
    uint32_t flags = 0u;
    uint32_t stream_word = 0u;
    uint32_t table_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, cpu->registers[5] + UINT32_C(0x10), &flags
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine, cpu->registers[5] + UINT32_C(2), &raw_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x14), &cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x18), &cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[7] = flags;
    cpu->registers[8] = (uint32_t)(int32_t)(int16_t)raw_count;
    if ((flags & (UINT32_C(1) << 3u)) != 0u ||
        (flags & (UINT32_C(1) << 4u)) != 0u ||
        (int32_t)cpu->registers[8] <= 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, VF2_TEXTURE_ACTIVE_FLAGS, flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[9], &stream_word);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[6] = 1u;
    cpu->registers[18] = stream_word;
    cpu->registers[24] = (stream_word ^ flags) & UINT32_C(1);
    table_index = (stream_word & UINT32_C(0xffff)) >> 1u;
    cpu->registers[16] = table_index;
    status = read_u16(
        machine,
        VF2_TEXTURE_COORD_TABLE + table_index * UINT32_C(4),
        &raw_x
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine,
            VF2_TEXTURE_COORD_TABLE + table_index * UINT32_C(4) + UINT32_C(2),
            &raw_y
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[22] = (uint32_t)(int32_t)(int16_t)raw_x;
    cpu->registers[23] = (uint32_t)(int32_t)(int16_t)raw_y;
    cpu->registers[16] = stream_word >> 24u;
    cpu->registers[17] = (stream_word >> 16u) & UINT32_C(0xff);
    cpu->registers[22] += cpu->registers[16];
    cpu->registers[23] += cpu->registers[17];

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ACTIVE_PREPARE_TARGET,
        VF2_TEXTURE_ACTIVE_PREPARE_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(22);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL;
    report->entry_address = VF2_TEXTURE_ACTIVE_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_ACTIVE_PREPARE_TARGET;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(22);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


'''
replace_once(
    "src/recovered/texture_bridge.c",
    "static vf2_status execute_texture_status_dispatch_call(\n",
    active_prepare_function + "static vf2_status execute_texture_status_dispatch_call(\n",
    "active-prepare implementation",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_TEXTURE_STATUS_DISPATCH_ENTRY:\n",
    "    case VF2_TEXTURE_ACTIVE_PREPARE_ENTRY:\n"
    "        status = execute_texture_active_prepare_call(\n"
    "            machine, cpu, &local_report\n"
    "        );\n"
    "        break;\n"
    "    case VF2_TEXTURE_STATUS_DISPATCH_ENTRY:\n",
    "active-prepare dispatcher",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL:\n"
    "        return \"texture-status-dispatch-call\";\n",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL:\n"
    "        return \"texture-status-dispatch-call\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL:\n"
    "        return \"texture-active-prepare-call\";\n",
    "active-prepare name",
)

replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "#include <stdint.h>\n#include <stdio.h>\n",
    "#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n",
    "stdlib include",
)

active_prepare_test = r'''static void test_active_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint32_t active_flags = UINT32_MAX;
    const uint32_t record = UINT32_C(0x00550168);
    const uint32_t stream = UINT32_C(0x00551000);
    const uint32_t stream_word = UINT32_C(0x12340004);
    const uint8_t count[2] = {3u, 0u};
    const size_t rom_size = (size_t)UINT32_C(0x0004c200);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0004c128)] = 10u;
    rom[UINT32_C(0x0004c129)] = 0u;
    rom[UINT32_C(0x0004c12a)] = 0xfdu;
    rom[UINT32_C(0x0004c12b)] = 0xffu;
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x10), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, record + UINT32_C(2), count, sizeof(count)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x14), stream
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x18), UINT32_C(0x00552000)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, stream, stream_word) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004bde0));
    cpu.registers[5] = record;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL);
    CHECK(report.entry_address == UINT32_C(0x0004bde0));
    CHECK(report.exit_address == UINT32_C(0x0004d16c));
    CHECK(report.recovered_instruction_count == UINT64_C(22));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.bytes_written == 4u);
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004d16c));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(22));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.local_frames[1].registers[2] == UINT32_C(0x0004be6c));
    CHECK(cpu.local_frames[1].registers[5] == record);
    CHECK(cpu.local_frames[1].registers[6] == 1u);
    CHECK(cpu.local_frames[1].registers[7] == 0u);
    CHECK(cpu.local_frames[1].registers[8] == 3u);
    CHECK(cpu.local_frames[1].registers[9] == stream);
    CHECK(cpu.local_frames[1].registers[10] == UINT32_C(0x00552000));
    CHECK(cpu.registers[16] == UINT32_C(0x12));
    CHECK(cpu.registers[17] == UINT32_C(0x34));
    CHECK(cpu.registers[18] == stream_word);
    CHECK(cpu.registers[22] == UINT32_C(28));
    CHECK(cpu.registers[23] == UINT32_C(49));
    CHECK(cpu.registers[24] == 0u);
    CHECK(cpu.arithmetic_control == arithmetic_before);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c2f4), &active_flags
        ) == VF2_OK
    );
    CHECK(active_flags == 0u);

    free(rom);
    vf2_model2a_shutdown(&machine);
}


'''
replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "static void test_record_advance_dispatch(void)\n",
    active_prepare_test + "static void test_record_advance_dispatch(void)\n",
    "active-prepare test",
)
replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "    test_loop_gate_dispatch();\n"
    "    test_record_advance_dispatch();\n",
    "    test_loop_gate_dispatch();\n"
    "    test_active_prepare_dispatch();\n"
    "    test_record_advance_dispatch();\n",
    "active-prepare test invocation",
)

replace_once(
    "tools/vf2i960/main.c",
    "                native_ip_before == UINT32_C(0x0004bd24) ||\n"
    "                native_ip_before == UINT32_C(0x0004bebc) ||\n",
    "                native_ip_before == UINT32_C(0x0004bd24) ||\n"
    "                native_ip_before == UINT32_C(0x0004bde0) ||\n"
    "                native_ip_before == UINT32_C(0x0004bebc) ||\n",
    "active-prepare candidate",
)
replace_once(
    "tools/vf2i960/main.c",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END ||\n",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END ||\n",
    "active-prepare checkpoint",
)
replace_once(
    "tools/vf2i960/main.c",
    "bridge_recovered_instructions != UINT64_C(1269003) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1819) ||\n"
    "             bridge_validated_blocks != 172u ||\n"
    "             bridge_memory_checkpoints != 172u ||\n"
    "             bridge_recovered_calls != UINT64_C(266) ||\n",
    "bridge_recovered_instructions != UINT64_C(1269091) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1731) ||\n"
    "             bridge_validated_blocks != 176u ||\n"
    "             bridge_memory_checkpoints != 176u ||\n"
    "             bridge_recovered_calls != UINT64_C(270) ||\n",
    "active-prepare totals",
)
replace_once(
    "tools/vf2i960/main.c",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END] != 1u ||\n",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END] != 1u ||\n",
    "active-prepare count assertion",
)
replace_once(
    "tools/vf2i960/main.c",
    "        printf(\"  status dispatch/calls:             %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL\n"
    "               ]);\n",
    "        printf(\"  status dispatch/calls:             %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL\n"
    "               ]);\n"
    "        printf(\"  active prepare/calls:              %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL\n"
    "               ]);\n",
    "active-prepare output",
)

catalog = Path("decomp/i960/functions.csv")
lines = catalog.read_text(encoding="utf-8").splitlines()
entry = (
    "0x0004bde0,0x0004d16c,texture_active_prepare_call,"
    "recovered-observed-branch,dynamic-differential+unit,"
    "Loads the active record and coordinate table state then enters the "
    "address-table helper in twenty-two instructions; observed four times"
)
if any(line.startswith("0x0004bde0,0x0004d16c,texture_active_prepare_call,") for line in lines):
    raise SystemExit("catalog entry already exists")
for index, line in enumerate(lines):
    if line.startswith("0x0004bd24,0x0004d2c0,texture_status_dispatch_call,"):
        lines.insert(index + 1, entry)
        break
else:
    raise SystemExit("status-dispatch catalog anchor not found")
catalog.write_text("\n".join(lines) + "\n", encoding="utf-8")

Path("decomp/i960/notes/texture_active_prepare_v0024.md").write_text(
    """# v0.0.24 texture active-record preparation

## Observed path

At `0x0004bde0`, the orchestrator loads the active record flags, remaining
count, and two stream pointers. The observed flags have bits 3 and 4 clear. It
reads the current stream word, derives an index into the signed coordinate
table at `0x0004c120`, adds the packed high-byte offsets, and calls the already
recovered address-table helper at `0x0004d16c`.

The path executes 22 instructions and one call per visit. It occurs four times,
recovering 88 instructions and four procedure calls. One 32-bit active-flags
word is written per visit. Unsupported flag/count branches remain rejected.

## Validation

The bridge test uses a synthetic ROM table and verifies the saved caller frame,
all affected global registers, the active-flags write, instruction/call
accounting, and the exact child target and return address. The block preserves
the incoming comparison and arithmetic-control state because its BBS/BBC
instructions are direct COBR comparisons.

The exact VF2 2.1 ROM-backed `native-second-dispatch` validator executed 22
reference instructions for each of the four visits and reached complete CPU
and Model 2 memory `MATCH`.

The strict totals are now 1,269,091 recovered and 1,731 interpreted
instructions across 176 recovered blocks and memory checkpoints. Recovered
calls and returns are 270 / 300.
""",
    encoding="utf-8",
)

Path(WORKFLOW).unlink()
Path(SCRIPT).unlink()
