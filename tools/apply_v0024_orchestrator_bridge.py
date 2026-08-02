from pathlib import Path
import re

WORKFLOW = ".github/workflows/integrate-v0024-orchestrator-bridge.yml"
SCRIPT = "tools/apply_v0024_orchestrator_bridge.py"


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


def regex_replace_once(
    path: str, pattern: str, replacement: str, label: str
) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    file.write_text(text, encoding="utf-8")


replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL,\n",
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE,\n"
    "    VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL,\n",
    "bridge candidate enum",
)
replace_once(
    "include/vf2/hybrid.h",
    "    VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY\n"
    "} vf2_hybrid_bridge_kind;\n",
    "    VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY,\n"
    "    VF2_HYBRID_BRIDGE_COUNT\n"
    "} vf2_hybrid_bridge_kind;\n",
    "bridge count sentinel",
)

replace_once(
    "src/recovered/texture_bridge.c",
    '#include "vf2/analysis/orchestrator_limits.h"\n',
    '#include "vf2/analysis/orchestrator_gates.h"\n'
    '#include "vf2/analysis/orchestrator_limits.h"\n'
    '#include "vf2/analysis/orchestrator_scan.h"\n',
    "bridge analysis includes",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "        cpu->registers[3] =\n"
    "            (uint32_t)(int32_t)(int16_t)active_count;\n"
    "        if (cpu->registers[3] == 0u) {\n"
    "            return VF2_ERROR_UNSUPPORTED;\n"
    "        }\n"
    "        status = read_u16(\n"
    "            machine, VF2_TEXTURE_RECORD_START, &status_value\n"
    "        );\n",
    "        cpu->registers[3] =\n"
    "            (uint32_t)(int32_t)(int16_t)active_count;\n"
    "        if (cpu->registers[3] == 0u) {\n"
    "            vf2_orchestrator_scan_report scan_report;\n"
    "            memset(&scan_report, 0, sizeof(scan_report));\n"
    "            status = vf2_orchestrator_scan_inactive_records(\n"
    "                machine, cpu, &scan_report\n"
    "            );\n"
    "            if (status != VF2_OK) {\n"
    "                return status;\n"
    "            }\n"
    "            report->kind =\n"
    "                VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END;\n"
    "            report->entry_address = scan_report.entry_address;\n"
    "            report->exit_address = scan_report.exit_address;\n"
    "            report->iterations =\n"
    "                (uint64_t)scan_report.records_scanned;\n"
    "            report->recovered_instruction_count =\n"
    "                scan_report.recovered_instruction_count;\n"
    "            report->cpu_poststate_applied =\n"
    "                scan_report.cpu_poststate_applied;\n"
    "            return VF2_OK;\n"
    "        }\n"
    "        status = read_u16(\n"
    "            machine, VF2_TEXTURE_RECORD_START, &status_value\n"
    "        );\n",
    "inactive scan delegation",
)

bridge_gate_helper = r'''static vf2_status execute_texture_orchestrator_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_orchestrator_gate_report gate_report;
    vf2_status status = VF2_OK;

    memset(&gate_report, 0, sizeof(gate_report));
    if (cpu->ip == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY) {
        status = vf2_orchestrator_apply_zero_loop_gate(
            machine, cpu, &gate_report
        );
    } else {
        status = vf2_orchestrator_enter_zero_child_gate(
            machine, cpu, &gate_report
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    switch (gate_report.kind) {
    case VF2_ORCHESTRATOR_GATE_CHILD_A:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A;
        break;
    case VF2_ORCHESTRATOR_GATE_CHILD_B:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B;
        break;
    case VF2_ORCHESTRATOR_GATE_LOOP_TAIL:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE;
        break;
    case VF2_ORCHESTRATOR_GATE_NONE:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    report->entry_address = gate_report.entry_address;
    report->exit_address = gate_report.exit_address;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count =
        gate_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        gate_report.recovered_procedure_calls;
    report->cpu_poststate_applied = gate_report.cpu_poststate_applied;
    return VF2_OK;
}

'''
replace_once(
    "src/recovered/texture_bridge.c",
    "vf2_status vf2_hybrid_post_frame_bridge_execute(\n",
    bridge_gate_helper + "vf2_status vf2_hybrid_post_frame_bridge_execute(\n",
    "bridge gate helper insertion",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_TEXTURE_STATUS_DISPATCH_ENTRY:\n"
    "        status = execute_texture_status_dispatch_call(\n"
    "            machine, cpu, &local_report\n"
    "        );\n"
    "        break;\n",
    "    case VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY:\n"
    "    case VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY:\n"
    "    case VF2_ORCHESTRATOR_LOOP_GATE_ENTRY:\n"
    "        status = execute_texture_orchestrator_gate(\n"
    "            machine, cpu, &local_report\n"
    "        );\n"
    "        break;\n"
    "    case VF2_TEXTURE_STATUS_DISPATCH_ENTRY:\n"
    "        status = execute_texture_status_dispatch_call(\n"
    "            machine, cpu, &local_report\n"
    "        );\n"
    "        break;\n",
    "bridge gate dispatcher cases",
)

replace_once(
    "src/recovered/texture_bridge.c",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL:\n"
    "        return \"texture-status-dispatch-call\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL:\n",
    "    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL:\n"
    "        return \"texture-status-dispatch-call\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END:\n"
    "        return \"texture-status-scan-end\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A:\n"
    "        return \"texture-child-gate-a\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B:\n"
    "        return \"texture-child-gate-b\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE:\n"
    "        return \"texture-loop-gate\";\n"
    "    case VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL:\n",
    "bridge kind names",
)

replace_once(
    "tools/vf2i960/main.c",
    "    size_t bridge_block_counts[64] = {0u};\n",
    "    size_t bridge_block_counts[VF2_HYBRID_BRIDGE_COUNT] = {0u};\n",
    "bridge count array",
)
replace_once(
    "tools/vf2i960/main.c",
    "                native_ip_before == UINT32_C(0x0004bd24) ||\n"
    "                native_ip_before == UINT32_C(0x0004bf90) ||\n",
    "                native_ip_before == UINT32_C(0x0004bd24) ||\n"
    "                native_ip_before == UINT32_C(0x0004bebc) ||\n"
    "                native_ip_before == UINT32_C(0x0004bef4) ||\n"
    "                native_ip_before == UINT32_C(0x0004bf2c) ||\n"
    "                native_ip_before == UINT32_C(0x0004bf90) ||\n",
    "bridge gate candidate addresses",
)
replace_once(
    "tools/vf2i960/main.c",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL ||\n",
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE ||\n"
    "                        bridge_report.kind ==\n"
    "                            VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL ||\n",
    "bridge checkpoint kinds",
)
replace_once(
    "tools/vf2i960/main.c",
    "             bridge_recovered_instructions != UINT64_C(1268866) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1956) ||\n"
    "             bridge_validated_blocks != 154u ||\n"
    "             bridge_memory_checkpoints != 154u ||\n"
    "             bridge_recovered_calls != UINT64_C(258) ||\n"
    "             bridge_recovered_returns != UINT64_C(300) ||\n",
    "             bridge_recovered_instructions != UINT64_C(1268941) ||\n"
    "             bridge_interpreted_instructions != UINT64_C(1881) ||\n"
    "             bridge_validated_blocks != 167u ||\n"
    "             bridge_memory_checkpoints != 167u ||\n"
    "             bridge_recovered_calls != UINT64_C(266) ||\n"
    "             bridge_recovered_returns != UINT64_C(300) ||\n",
    "strict bridge totals",
)
replace_once(
    "tools/vf2i960/main.c",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL] != 1u ||\n",
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END] != 1u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE] != 4u ||\n"
    "             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL] != 1u ||\n",
    "strict bridge block counts",
)
replace_once(
    "tools/vf2i960/main.c",
    "        printf(\"  final status/calls:                %zu\\n\",\n",
    "        printf(\"  status scan endings:              %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END\n"
    "               ]);\n"
    "        printf(\"  child gate A calls:                %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A\n"
    "               ]);\n"
    "        printf(\"  child gate B calls:                %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B\n"
    "               ]);\n"
    "        printf(\"  loop zero gates:                   %zu\\n\",\n"
    "               bridge_block_counts[\n"
    "                   VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE\n"
    "               ]);\n"
    "        printf(\"  final status/calls:                %zu\\n\",\n",
    "bridge summary counts",
)

replace_once(
    "CMakeLists.txt",
    "    if(VF2_ROM_DIR)\n",
    "    add_executable(vf2_orchestrator_bridge_tests\n"
    "        tests/analysis/test_orchestrator_bridge.c\n"
    "    )\n"
    "    target_link_libraries(vf2_orchestrator_bridge_tests PRIVATE vf2_core)\n"
    "    vf2_set_project_warnings(vf2_orchestrator_bridge_tests)\n\n"
    "    add_test(\n"
    "        NAME vf2_orchestrator_bridge\n"
    "        COMMAND vf2_orchestrator_bridge_tests\n"
    "    )\n\n"
    "    if(VF2_ROM_DIR)\n",
    "bridge integration test target",
)

replace_once(
    "decomp/i960/functions.csv",
    "0x0004bd24,0x0004bf90,texture_status_scan_end,candidate-semantic,dynamic-trace+unit,Final pass scans ten inactive records in 43 instructions; pending live bridge integration\n",
    "0x0004bd24,0x0004bf90,texture_status_scan_end,recovered-observed-branch,pending-rom-differential+unit,Final pass scans ten inactive records in 43 instructions through the hybrid bridge\n",
    "scan catalog promotion",
)
replace_once(
    "decomp/i960/functions.csv",
    "0x0004bebc,0x0004cb64,texture_child_zero_gate_a,candidate-semantic,dynamic-trace+unit,Zero child-state path enters 0x0004cb64 in three instructions; observed four times\n",
    "0x0004bebc,0x0004cb64,texture_child_zero_gate_a,recovered-observed-branch,pending-rom-differential+unit,Zero child-state path enters 0x0004cb64 in three instructions through the hybrid bridge; observed four times\n",
    "gate A catalog promotion",
)
replace_once(
    "decomp/i960/functions.csv",
    "0x0004bef4,0x0004cd18,texture_child_zero_gate_b,candidate-semantic,dynamic-trace+unit,Zero child-state path enters 0x0004cd18 in three instructions; observed four times\n",
    "0x0004bef4,0x0004cd18,texture_child_zero_gate_b,recovered-observed-branch,pending-rom-differential+unit,Zero child-state path enters 0x0004cd18 in three instructions through the hybrid bridge; observed four times\n",
    "gate B catalog promotion",
)
replace_once(
    "decomp/i960/functions.csv",
    "0x0004bf2c,0x0004bf60,texture_loop_zero_gate,candidate-semantic,dynamic-trace+unit,Zero child-state loop-tail branch in two instructions; observed four times\n",
    "0x0004bf2c,0x0004bf60,texture_loop_zero_gate,recovered-observed-branch,pending-rom-differential+unit,Zero child-state loop-tail branch in two instructions through the hybrid bridge; observed four times\n",
    "loop gate catalog promotion",
)

for path in (WORKFLOW, SCRIPT):
    file = Path(path)
    if file.exists():
        file.unlink()
