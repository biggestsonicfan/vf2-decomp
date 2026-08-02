from pathlib import Path


WORKFLOW = ".github/workflows/integrate-v0024-counter-update.yml"
SCRIPT = "tools/apply_v0024_counter_update.py"


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE,\n",
    "    VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE,\n",
    "counter-update enum",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "#define VF2_TEXTURE_POST_BODY_CALL_RETURN UINT32_C(0x0004bb98)\n"
    "#define VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY UINT32_C(0x0004bc58)\n",
    "#define VF2_TEXTURE_POST_BODY_CALL_RETURN UINT32_C(0x0004bb98)\n"
    "#define VF2_TEXTURE_COUNTER_UPDATE_ENTRY UINT32_C(0x0004bb98)\n"
    "#define VF2_TEXTURE_COUNTER_UPDATE_EXIT UINT32_C(0x0004bc58)\n"
    "#define VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY UINT32_C(0x0004bc58)\n",
    "counter-update constants",
)

counter_update = r'''static vf2_status execute_texture_counter_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t counter0 = 0u;
    uint32_t counter1 = 0u;
    uint32_t counter2 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER0;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER0, &counter0);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter0 - UINT32_C(1);
    if (counter0 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER1;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER1, &counter1);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter1 - UINT32_C(1);
    if (counter1 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER2;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER2, &counter2);
    if (status != VF2_OK) {
        return status;
    }
    if (counter2 <= UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[4] = counter2 - UINT32_C(1);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
    status = vf2_model2a_write_u32(
        machine, VF2_TEXTURE_COUNTER2, cpu->registers[4]
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
    cpu->executed_instructions += UINT64_C(14);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE;
    report->entry_address = VF2_TEXTURE_COUNTER_UPDATE_ENTRY;
    report->exit_address = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
    report->iterations = UINT64_C(3);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(14);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


'''

replace_once(
    "src/recovered/texture_bridge.c",
    "static vf2_status execute_texture_orchestrator_epilogue(\n",
    counter_update + "static vf2_status execute_texture_orchestrator_epilogue(\n",
    "counter-update implementation",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_TEXTURE_POST_BODY_CALL_ENTRY:\n"
    "        status = execute_texture_post_body_call(\n"
    "            cpu, &local_report\n"
    "        );\n"
    "        break;\n"
    "    case VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY:\n",
    "    case VF2_TEXTURE_POST_BODY_CALL_ENTRY:\n"
    "        status = execute_texture_post_body_call(\n"
    "            cpu, &local_report\n"
    "        );\n"
    "        break;\n"
    "    case VF2_TEXTURE_COUNTER_UPDATE_ENTRY:\n"
    "        status = execute_texture_counter_update(\n"
    "            machine, cpu, &local_report\n"
    "        );\n"
    "        break;\n"
    "    case VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY:\n",
    "counter-update dispatch",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL:\n"
    "        return \"texture-post-body-call\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE:\n",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL:\n"
    "        return \"texture-post-body-call\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE:\n"
    "        return \"texture-counter-update\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE:\n",
    "counter-update name",
)

replace_once(
    "tools/vf2i960/main.c",
    "                native_ip_before == UINT32_C(0x0004bb94) ||\n"
    "                native_ip_before == UINT32_C(0x0004bc58) ||\n",
    "                native_ip_before == UINT32_C(0x0004bb94) ||\n"
    "                native_ip_before == UINT32_C(0x0004bb98) ||\n"
    "                native_ip_before == UINT32_C(0x0004bc58) ||\n",
    "counter-update candidate address",
)

replace_once(
    "tools/vf2i960/main.c",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE ||\n",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE ||\n",
    "counter-update memory checkpoint",
)

replace_once(
    "tools/vf2i960/main.c",
    "             bridge_recovered_instructions != UINT64_C(1268941) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1881) ||\n"
    "             bridge_validated_blocks != 167u ||\n"
    "             bridge_memory_checkpoints != 167u ||\n",
    "             bridge_recovered_instructions != UINT64_C(1268955) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1867) ||\n"
    "             bridge_validated_blocks != 168u ||\n"
    "             bridge_memory_checkpoints != 168u ||\n",
    "strict counter-update totals",
)

replace_once(
    "tools/vf2i960/main.c",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL] != 1u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE] != 1u ||\n",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL] != 1u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE] != 1u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE] != 1u ||\n",
    "strict counter-update block count",
)

replace_once(
    "tools/vf2i960/main.c",
    "        printf(\"  orchestrator epilogues:            %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE\n"
    "               ]);\n",
    "        printf(\"  counter updates:                   %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE\n"
    "               ]);\n"
    "        printf(\"  orchestrator epilogues:            %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE\n"
    "               ]);\n",
    "counter-update summary",
)

replace_once(
    "decomp/i960/functions.csv",
    "0x0004bb94,0x0004b8d8,texture_post_body_call,recovered-control-block,dynamic-trace+unit,Post-body call into the short maintenance helper\n",
    "0x0004bb94,0x0004b8d8,texture_post_body_call,recovered-control-block,dynamic-trace+unit,Post-body call into the short maintenance helper\n"
    "0x0004bb98,0x0004bc58,texture_counter_update,recovered-observed-branch,pending-rom-differential+unit,Two zero counters are skipped and the third counter is decremented in fourteen instructions\n",
    "counter-update function catalog",
)

test_function = r'''static void test_counter_update_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t counter0 = UINT32_MAX;
    uint32_t counter1 = UINT32_MAX;
    uint32_t counter2 = UINT32_MAX;
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502c0), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502d0), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502e0), UINT32_C(3)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bb98));
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x0004bb98));
    CHECK(report.exit_address == UINT32_C(0x0004bc58));
    CHECK(report.iterations == UINT64_C(3));
    CHECK(report.changed_values == UINT64_C(1));
    CHECK(report.bytes_written == 4u);
    CHECK(report.recovered_instruction_count == UINT64_C(14));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bc58));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(14));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[3] == UINT32_C(0x005502e0));
    CHECK(cpu.registers[4] == UINT32_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(4))
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502c0), &counter0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502d0), &counter1
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502e0), &counter2
        ) == VF2_OK
    );
    CHECK(counter0 == 0u);
    CHECK(counter1 == 0u);
    CHECK(counter2 == UINT32_C(2));

    vf2_model2a_shutdown(&machine);
}


'''

replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "int main(void)\n{\n",
    test_function + "int main(void)\n{\n",
    "counter-update test function",
)

replace_once(
    "tests/analysis/test_orchestrator_bridge.c",
    "    test_loop_gate_dispatch();\n\n"
    "    if (failures != 0) {\n",
    "    test_loop_gate_dispatch();\n"
    "    test_counter_update_dispatch();\n\n"
    "    if (failures != 0) {\n",
    "counter-update test call",
)

Path("decomp/i960/notes/texture_counter_update_v0024.md").write_text(
    '''# v0.0.24 texture counter update

## Observed path

After the post-body maintenance call returns to `0x0004bb98`, the
orchestrator examines three 32-bit counters:

- `0x005502c0` is zero, so the decremented register value is discarded;
- `0x005502d0` is zero, so the decremented register value is discarded;
- `0x005502e0` is greater than one, so it is decremented and written back.

The block reaches `0x0004bc58` after exactly 14 instructions. It performs one
32-bit memory write and no procedure calls or returns.

## i960 semantics

`cmpdeco 1, r4, r4` compares the literal one with the original value of `r4`
using an unsigned comparison, then stores `r4 - 1` in the destination. The
final observed comparison is `less`, because one is less than the third
counter's original value.

## Validation

The public bridge test verifies the instruction count, final IP, `r3`, `r4`,
comparison and arithmetic-control state, unchanged first two counters, and the
single decrement of the third counter.

The strict bounded bridge expectation is now 1,268,955 recovered and 1,867
interpreted instructions across 168 recovered blocks and memory checkpoints.
A full ROM-backed `native-second-dispatch MATCH` remains required for final
dynamic-differential promotion.
''',
    encoding="utf-8",
)

Path(WORKFLOW).unlink()
Path(SCRIPT).unlink()
