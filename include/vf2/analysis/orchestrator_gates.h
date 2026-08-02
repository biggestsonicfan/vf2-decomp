#ifndef VF2_ANALYSIS_ORCHESTRATOR_GATES_H
#define VF2_ANALYSIS_ORCHESTRATOR_GATES_H

#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

enum {
    VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY = 0x0004bebcu,
    VF2_ORCHESTRATOR_CHILD_GATE_A_TARGET = 0x0004cb64u,
    VF2_ORCHESTRATOR_CHILD_GATE_A_RETURN = 0x0004bef4u,
    VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY = 0x0004bef4u,
    VF2_ORCHESTRATOR_CHILD_GATE_B_TARGET = 0x0004cd18u,
    VF2_ORCHESTRATOR_CHILD_GATE_B_RETURN = 0x0004bf2cu,
    VF2_ORCHESTRATOR_LOOP_GATE_ENTRY = 0x0004bf2cu,
    VF2_ORCHESTRATOR_LOOP_GATE_EXIT = 0x0004bf60u,
    VF2_ORCHESTRATOR_CHILD_STATE = 0x00550080u
};

typedef enum vf2_orchestrator_gate_kind {
    VF2_ORCHESTRATOR_GATE_NONE = 0,
    VF2_ORCHESTRATOR_GATE_CHILD_A,
    VF2_ORCHESTRATOR_GATE_CHILD_B,
    VF2_ORCHESTRATOR_GATE_LOOP_TAIL
} vf2_orchestrator_gate_kind;

typedef struct vf2_orchestrator_gate_report {
    vf2_orchestrator_gate_kind kind;
    uint32_t entry_address;
    uint32_t exit_address;
    uint32_t child_state;
    uint32_t call_target;
    uint32_t return_address;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    int cpu_poststate_applied;
} vf2_orchestrator_gate_report;

/*
 * Recover either observed three-instruction zero-state gate that enters a
 * texture child procedure. The CPU IP selects gate A or B.
 */
vf2_status vf2_orchestrator_enter_zero_child_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_orchestrator_gate_report *report
);

/*
 * Recover the observed two-instruction zero-state loop-tail gate from
 * 0x0004bf2c to 0x0004bf60.
 */
vf2_status vf2_orchestrator_apply_zero_loop_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_orchestrator_gate_report *report
);

#endif
