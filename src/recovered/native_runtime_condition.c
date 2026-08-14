#include "vf2/native_runtime.h"

#define VF2_INTERRUPT_PLAYER_LAYER_ENTRY UINT32_C(0x00000c78)
#define VF2_INTERRUPT_GAME_INPUT_ENTRY UINT32_C(0x00000c80)
#define VF2_MAIN_POST_TIMER_ENTRY UINT32_C(0x0000a038)
#define VF2_MAIN_CLEAR_PREFIX_ENTRY UINT32_C(0x00009fb0)
#define VF2_FRAME_SELECTOR UINT32_C(0x0050002a)

vf2_status vf2_native_runtime_step_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
);

vf2_status vf2_hybrid_bridge_apply_condition_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t entry_r3,
    uint32_t entry_r7
);

static void set_none_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_NONE;
}

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

static void set_greater_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
}

static vf2_status read_frame_selector(
    vf2_model2a *machine,
    uint8_t *selector
)
{
    return vf2_model2a_read(
        machine,
        VF2_FRAME_SELECTOR,
        selector,
        sizeof(*selector)
    );
}

static vf2_status apply_repeated_bridge_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry
)
{
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    if (entry == VF2_INTERRUPT_PLAYER_LAYER_ENTRY &&
        cpu->ip == VF2_INTERRUPT_GAME_INPUT_ENTRY) {
        status = read_frame_selector(machine, &selector);
        if (status != VF2_OK) {
            return status;
        }
        if (selector == UINT8_C(17)) {
            set_less_condition(cpu);
        } else if (selector == UINT8_C(1) || selector == UINT8_C(16)) {
            set_greater_condition(cpu);
        }
    } else if (entry == VF2_MAIN_POST_TIMER_ENTRY &&
               cpu->ip == VF2_MAIN_CLEAR_PREFIX_ENTRY) {
        status = read_frame_selector(machine, &selector);
        if (status != VF2_OK) {
            return status;
        }
        if (selector == UINT8_C(17)) {
            set_less_condition(cpu);
        } else if (selector == UINT8_C(1) || selector == UINT8_C(16)) {
            set_equal_condition(cpu);
        }
    }
    return VF2_OK;
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
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    const uint32_t entry_r3 = cpu != NULL ? cpu->registers[3] : 0u;
    const uint32_t entry_r7 = cpu != NULL ? cpu->registers[7] : 0u;
    vf2_status status = vf2_native_runtime_step_impl(
        machine, cpu, state, effective_report
    );

    if (status != VF2_OK) {
        return status;
    }
    if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE) {
        status = vf2_hybrid_bridge_apply_condition_poststate(
            machine, cpu, entry, entry_r3, entry_r7
        );
        if (status == VF2_OK) {
            status = apply_repeated_bridge_condition(
                machine, cpu, entry
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    } else if (effective_report->kind ==
                   VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION ||
               effective_report->kind ==
                   VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH ||
               (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
                effective_report->task_kind == VF2_HYBRID_TASK_CAMERA)) {
        set_equal_condition(cpu);
    } else if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
               effective_report->task_kind == VF2_HYBRID_TASK_KILL_OSAGE) {
        set_less_condition(cpu);
    } else if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
               (effective_report->task_kind == VF2_HYBRID_TASK_OSAGE0 ||
                effective_report->task_kind == VF2_HYBRID_TASK_OSAGE1)) {
        set_none_condition(cpu);
    }
    return VF2_OK;
}
