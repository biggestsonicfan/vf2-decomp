#include "vf2/analysis/orchestrator_gates.h"

#include <string.h>

#define VF2_ORCHESTRATOR_CHILD_GATE_INSTRUCTIONS UINT64_C(3)
#define VF2_ORCHESTRATOR_LOOP_GATE_INSTRUCTIONS UINT64_C(2)

static vf2_status read_child_state(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t *child_state
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || child_state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, VF2_ORCHESTRATOR_CHILD_STATE, child_state
    );
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = *child_state;
    }
    return status;
}

vf2_status vf2_orchestrator_enter_zero_child_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_orchestrator_gate_report *report
)
{
    vf2_orchestrator_gate_report local_report;
    uint32_t child_state = 0u;
    uint32_t target = 0u;
    uint32_t return_address = 0u;
    vf2_orchestrator_gate_kind kind = VF2_ORCHESTRATOR_GATE_NONE;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    switch (cpu->ip) {
    case VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY:
        kind = VF2_ORCHESTRATOR_GATE_CHILD_A;
        target = VF2_ORCHESTRATOR_CHILD_GATE_A_TARGET;
        return_address = VF2_ORCHESTRATOR_CHILD_GATE_A_RETURN;
        break;
    case VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY:
        kind = VF2_ORCHESTRATOR_GATE_CHILD_B;
        target = VF2_ORCHESTRATOR_CHILD_GATE_B_TARGET;
        return_address = VF2_ORCHESTRATOR_CHILD_GATE_B_RETURN;
        break;
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    status = read_child_state(machine, cpu, &child_state);
    if (status != VF2_OK) {
        return status;
    }
    if (child_state != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions +=
        VF2_ORCHESTRATOR_CHILD_GATE_INSTRUCTIONS;

    memset(&local_report, 0, sizeof(local_report));
    local_report.kind = kind;
    local_report.entry_address =
        kind == VF2_ORCHESTRATOR_GATE_CHILD_A
            ? VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY
            : VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY;
    local_report.exit_address = target;
    local_report.child_state = child_state;
    local_report.call_target = target;
    local_report.return_address = return_address;
    local_report.recovered_instruction_count =
        VF2_ORCHESTRATOR_CHILD_GATE_INSTRUCTIONS;
    local_report.recovered_procedure_calls = UINT64_C(1);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_orchestrator_apply_zero_loop_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_orchestrator_gate_report *report
)
{
    vf2_orchestrator_gate_report local_report;
    uint32_t child_state = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_ORCHESTRATOR_LOOP_GATE_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = read_child_state(machine, cpu, &child_state);
    if (status != VF2_OK) {
        return status;
    }
    if (child_state != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->ip = VF2_ORCHESTRATOR_LOOP_GATE_EXIT;
    cpu->executed_instructions += VF2_ORCHESTRATOR_LOOP_GATE_INSTRUCTIONS;

    memset(&local_report, 0, sizeof(local_report));
    local_report.kind = VF2_ORCHESTRATOR_GATE_LOOP_TAIL;
    local_report.entry_address = VF2_ORCHESTRATOR_LOOP_GATE_ENTRY;
    local_report.exit_address = VF2_ORCHESTRATOR_LOOP_GATE_EXIT;
    local_report.child_state = child_state;
    local_report.recovered_instruction_count =
        VF2_ORCHESTRATOR_LOOP_GATE_INSTRUCTIONS;
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
