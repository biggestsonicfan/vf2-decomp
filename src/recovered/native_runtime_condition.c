#define vf2_native_runtime_step vf2_native_runtime_step_selector2_impl
#include "native_runtime_condition_selector2_impl.c"
#undef vf2_native_runtime_step

static vf2_status capture_selector2_condition_input(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    int *selector2_path,
    uint8_t *queue_count,
    uint8_t *profile_mode
)
{
    uint8_t selector = UINT8_MAX;
    uint32_t base = 0u;
    vf2_status status = VF2_OK;

    *selector2_path = 0;
    *queue_count = 0u;
    *profile_mode = 0u;
    if (machine == NULL || cpu == NULL ||
        (entry != VF2_MAIN_FINAL_CLUSTER_ENTRY &&
         entry != VF2_FRAME_DISPATCH_TICK_ENTRY)) {
        return VF2_OK;
    }

    status = read_frame_selector(machine, &selector);
    if (status != VF2_OK || selector != UINT8_C(2)) {
        return status;
    }
    *selector2_path = 1;
    status = vf2_model2a_read(
        machine, UINT32_C(0x00504001), queue_count, sizeof(*queue_count)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            base + UINT32_C(0x3351),
            profile_mode,
            sizeof(*profile_mode)
        );
    }
    return status;
}

static void apply_selector2_final_condition(
    vf2_i960_cpu *cpu,
    uint8_t queue_count,
    uint8_t profile_mode
)
{
    /* The four 0x43888 calls all enter through cmpobe 0,(mask&0xc).
     * With selector 2 that compare is LESS.  If profile bit 0 is set the
     * helper returns from that branch and LESS survives.  Otherwise every
     * call reaches cmpobge count,16.  Queue entries increment while count is
     * below 16, so the fourth call compares: <16 for entry counts 0..12,
     * ==16 for 13..16, and >16 above that. */
    if ((profile_mode & UINT8_C(1)) != 0u || queue_count <= UINT8_C(12)) {
        set_runtime_less_condition(cpu);
    } else if (queue_count <= UINT8_C(16)) {
        set_runtime_equal_condition(cpu);
    } else {
        set_runtime_greater_condition(cpu);
    }
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    int selector2_path = 0;
    uint8_t queue_count = 0u;
    uint8_t profile_mode = 0u;
    vf2_status status = capture_selector2_condition_input(
        machine,
        cpu,
        entry,
        &selector2_path,
        &queue_count,
        &profile_mode
    );

    if (status != VF2_OK) {
        return status;
    }
    status = vf2_native_runtime_step_selector2_impl(
        machine, cpu, state, report
    );
    if (status == VF2_OK && selector2_path) {
        apply_selector2_final_condition(cpu, queue_count, profile_mode);
    }
    return status;
}
