#include "vf2/native_runtime.h"

vf2_status vf2_native_runtime_step_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
);

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report;
    vf2_status status = VF2_OK;

    local_report = (vf2_native_runtime_step_report){0};
    status = vf2_native_runtime_step_impl(
        machine, cpu, state, &local_report
    );
    if (status != VF2_OK) {
        return status;
    }

    if (local_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION) {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
    }

    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
