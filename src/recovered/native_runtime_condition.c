#include "vf2/native_runtime.h"

vf2_status vf2_native_runtime_step_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
);

static void set_equal_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
}

static void set_less_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    vf2_status status = vf2_native_runtime_step_impl(
        machine, cpu, state, effective_report
    );

    if (status != VF2_OK) {
        return status;
    }
    if (effective_report->kind ==
            VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION ||
        (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
         effective_report->task_kind == VF2_HYBRID_TASK_CAMERA)) {
        set_equal_condition(cpu);
    } else if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
               effective_report->task_kind == VF2_HYBRID_TASK_KILL_OSAGE) {
        set_less_condition(cpu);
    }
    return VF2_OK;
}
