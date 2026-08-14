#include "texture_bridge_internal.h"

vf2_status vf2_texture_entry_gate_with_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const vf2_status status = execute_texture_orchestrator_entry_gate(
        machine, cpu, report
    );

    if (status == VF2_OK) {
        set_equal_condition(cpu);
    }
    return status;
}
