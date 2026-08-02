from pathlib import Path

WORKFLOW = ".github/workflows/integrate-v0024-record-advance.yml"
SCRIPT = "tools/apply_v0024_record_advance.py"


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL,\n",
    "    VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL,\n",
    "record-advance enum",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "#define VF2_TEXTURE_FINAL_STATUS_ENTRY UINT32_C(0x0004bf90)\n",
    "#define VF2_TEXTURE_RECORD_ADVANCE_ENTRY UINT32_C(0x0004bf60)\n"
    "#define VF2_TEXTURE_RECORD_ADVANCE_EXIT UINT32_C(0x0004bd24)\n"
    "#define VF2_TEXTURE_FINAL_STATUS_ENTRY UINT32_C(0x0004bf90)\n",
    "record-advance constants",
)

record_advance_function = r'''static vf2_status execute_texture_record_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t raw_count = 0u;
    uint32_t count = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || cpu->registers[6] != 1u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = read_u16(machine, cpu->registers[5] + UINT32_C(2), &raw_count);
    if (status != VF2_OK) {
        return status;
    }
    count = (uint32_t)(int32_t)(int16_t)raw_count;
    cpu->registers[VF2_I960_G0_REGISTER] = count;
    if ((int32_t)count <= 0 || cpu->registers[8] != count) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[6] = 0u;
    cpu->registers[8] -= UINT32_C(1);
    cpu->registers[9] += UINT32_C(4);
    cpu->registers[10] += UINT32_C(4);

    status = write_u16(
        machine,
        cpu->registers[5] + UINT32_C(2),
        (uint16_t)cpu->registers[8]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[5] + UINT32_C(0x14),
            cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[5] + UINT32_C(0x18),
            cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    set_equal_condition(cpu);
    cpu->ip = VF2_TEXTURE_RECORD_ADVANCE_EXIT;
    cpu->executed_instructions += UINT64_C(12);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE;
    report->entry_address = VF2_TEXTURE_RECORD_ADVANCE_ENTRY;
    report->exit_address = VF2_TEXTURE_RECORD_ADVANCE_EXIT;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(3);
    report->bytes_written = 10u;
    report->recovered_instruction_count = UINT64_C(12);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


'''
replace_once(
    "src/recovered/texture_bridge.c",
    "static vf2_status execute_texture_final_status_call(\n",
    record_advance_function + "static vf2_status execute_texture_final_status_call(\n",
    "record-advance implementation",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_TEXTURE_FINAL_STATUS_ENTRY:\n",
    "    case VF2_TEXTURE_RECORD_ADVANCE_ENTRY:\n"
    "        status = execute_texture_record_advance(\n"
    "            machine, cpu, &local_report\n"
    "        );\n"
    "        break;\n"
    "    case VF2_TEXTURE_FINAL_STATUS_ENTRY:\n",
    "record-advance dispatcher",
)
replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE:\n"
    "        return \"texture-loop-gate\";\n",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE:\n"
    "        return \"texture-loop-gate\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE:\n"
    "        return \"texture-record-advance\";\n",
    "record-advance name",
)

record_advance_test = r'''static void test_record_advance_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t pointer0 = 0u;
    uint32_t pointer1 = 0u;
    uint8_t count_bytes[2] = {3u, 0u};
    uint8_t final_count[2] = {0u, 0u};
    const uint32_t record = UINT32_C(0x00550168);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write(
            &machine, record + UINT32_C(2), count_bytes, sizeof(count_bytes)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bf60));
    cpu.registers[5] = record;
    cpu.registers[6] = 1u;
    cpu.registers[8] = 3u;
    cpu.registers[9] = UINT32_C(0x00551000);
    cpu.registers[10] = UINT32_C(0x00552000);
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE);
    CHECK(report.entry_address == UINT32_C(0x0004bf60));
    CHECK(report.exit_address == UINT32_C(0x0004bd24));
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(3));
    CHECK(report.bytes_written == 10u);
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bd24));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(12));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(3));
    CHECK(cpu.registers[6] == 0u);
    CHECK(cpu.registers[8] == 2u);
    CHECK(cpu.registers[9] == UINT32_C(0x00551004));
    CHECK(cpu.registers[10] == UINT32_C(0x00552004));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(2))
    );
    CHECK(
        vf2_model2a_read(
            &machine, record + UINT32_C(2), final_count, sizeof(final_count)
        ) == VF2_OK
    );
    CHECK(final_count[0] == 2u && final_count[1] == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, record + UINT32_C(0x14), &pointer0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, record + UINT32_C(0x18), &pointer1
        ) == VF2_OK
    );
    CHECK(pointer0 == UINT32_C(0x00551004));
    CHECK(pointer1 == UINT32_C(0x00552004));

    vf2_model2a_shutdown(&machine);
}


'''
replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "static void test_counter_update_dispatch(void)\n",
    record_advance_test + "static void test_counter_update_dispatch(void)\n",
    "record-advance test",
)
replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "    test_loop_gate_dispatch();\n"
    "    test_counter_update_dispatch();\n",
    "    test_loop_gate_dispatch();\n"
    "    test_record_advance_dispatch();\n"
    "    test_counter_update_dispatch();\n",
    "record-advance test invocation",
)

replace_once(
    "tools/vf2i960/main.c",
    "                native_ip_before == UINT32_C(0x0004bf2c) ||\n"
    "                native_ip_before == UINT32_C(0x0004bf90) ||\n",
    "                native_ip_before == UINT32_C(0x0004bf2c) ||\n"
    "                native_ip_before == UINT32_C(0x0004bf60) ||\n"
    "                native_ip_before == UINT32_C(0x0004bf90) ||\n",
    "record-advance candidate",
)
replace_once(
    "tools/vf2i960/main.c",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL ||\n",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL ||\n",
    "record-advance checkpoint",
)
replace_once(
    "tools/vf2i960/main.c",
    "bridge_recovered_instructions != UINT64_C(1268955) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1867) ||\n"
    "             bridge_validated_blocks != 168u ||\n"
    "             bridge_memory_checkpoints != 168u ||\n",
    "bridge_recovered_instructions != UINT64_C(1269003) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1819) ||\n"
    "             bridge_validated_blocks != 172u ||\n"
    "             bridge_memory_checkpoints != 172u ||\n",
    "record-advance totals",
)
replace_once(
    "tools/vf2i960/main.c",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL] != 1u ||\n",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL] != 1u ||\n",
    "record-advance count assertion",
)
replace_once(
    "tools/vf2i960/main.c",
    "        printf(\"  loop zero gates:                   %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE\n"
    "               ]);\n",
    "        printf(\"  loop zero gates:                   %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE\n"
    "               ]);\n"
    "        printf(\"  record advances:                   %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE\n"
    "               ]);\n",
    "record-advance output",
)

catalog = Path("decomp/i960/functions.csv")
lines = catalog.read_text(encoding="utf-8").splitlines()
entry = (
    "0x0004bf60,0x0004bd24,texture_record_advance,"
    "recovered-observed-branch,dynamic-differential+unit,"
    "Positive record count decrements and advances both source pointers in "
    "twelve instructions; observed four times"
)
if any(line.startswith("0x0004bf60,0x0004bd24,texture_record_advance,") for line in lines):
    raise SystemExit("catalog entry already exists")
for index, line in enumerate(lines):
    if line.startswith("0x0004bf2c,0x0004bf60,texture_loop_zero_gate,"):
        lines.insert(index + 1, entry)
        break
else:
    raise SystemExit("loop-gate catalog anchor not found")
catalog.write_text("\n".join(lines) + "\n", encoding="utf-8")

Path("decomp/i960/notes/texture_record_advance_v0024.md").write_text(
    """# v0.0.24 texture record advance

## Observed path

The active texture-record loop reaches `0x0004bf60` four times. On every
visit, `r6` is one and the signed halfword at record offset `0x02` is positive.
The block decrements the remaining count, advances the two stream pointers by
four bytes, writes all three values back to the record, and branches to
`0x0004bd24`.

The exact observed path is 12 instructions per visit, for 48 recovered
instructions across four visits. It writes 10 bytes per visit and performs no
procedure calls or returns. Unsupported counter or loop states are rejected.

## i960 comparison semantics

`cmpdeco 1, r6, r6` compares one with the original value and then decrements
the destination. Because the observed original value is one, it leaves the
architectural comparison state equal. The following `cmpibg` and `cmpibe` are
COBR-format direct comparisons: they select branches without replacing that
comparison state. The recovery therefore preserves equal through the exit.

## Validation

The public bridge test covers the full CPU and memory post-state, including
`g0`, `r6`, `r8`, `r9`, `r10`, the preserved equal comparison from `cmpdeco`,
the halfword count, and both 32-bit pointers.

The exact VF2 2.1 ROM-backed `native-second-dispatch` validator executed 12
reference i960 instructions for each of the four visits and reached full CPU
and Model 2 memory `MATCH`. The strict totals are now 1,269,003 recovered and
1,819 interpreted instructions, with 172 recovered blocks and memory
checkpoints. Calls and returns remain 266 / 300.
""",
    encoding="utf-8",
)

Path(WORKFLOW).unlink()
Path(SCRIPT).unlink()
