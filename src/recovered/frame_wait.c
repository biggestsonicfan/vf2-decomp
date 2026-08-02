#include "vf2/hybrid.h"

#include <string.h>

#define VF2_FRAME_WAIT_EARLY UINT32_C(0x00000f7c)
#define VF2_FRAME_WAIT_MAIN UINT32_C(0x00010f98)
#define VF2_FRAME_INTERRUPT_MASK UINT32_C(1)
#define VF2_FRAME_INTERRUPT_VECTOR UINT32_C(12)
#define VF2_FRAME_INTERRUPT_LEVEL UINT32_C(1)

vf2_status vf2_hybrid_frame_wait_initialize(
    vf2_hybrid_frame_wait_state *state,
    size_t visits_before_interrupt
)
{
    if (state == NULL || visits_before_interrupt == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    state->visits_before_interrupt = visits_before_interrupt;
    return VF2_OK;
}

vf2_status vf2_hybrid_frame_wait_observe(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *state,
    vf2_hybrid_frame_wait_report *report
)
{
    vf2_hybrid_frame_wait_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL ||
        state->visits_before_interrupt == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    if (cpu->ip != VF2_FRAME_WAIT_EARLY &&
        cpu->ip != VF2_FRAME_WAIT_MAIN) {
        if (report != NULL) {
            *report = local_report;
        }
        return VF2_OK;
    }

    local_report.wait_address = cpu->ip;
    local_report.wait_observed = 1;
    ++state->visits;
    local_report.visit_count = state->visits;
    if (state->visits >= state->visits_before_interrupt) {
        status = vf2_model2a_raise_interrupt(
            machine, VF2_FRAME_INTERRUPT_MASK
        );
        if (status == VF2_OK) {
            status = vf2_i960_cpu_enter_interrupt(
                cpu,
                machine,
                VF2_FRAME_INTERRUPT_VECTOR,
                VF2_FRAME_INTERRUPT_LEVEL
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        state->visits = 0u;
        ++state->interrupts_injected;
        local_report.interrupt_vector = VF2_FRAME_INTERRUPT_VECTOR;
        local_report.interrupt_handler = cpu->ip;
        local_report.interrupt_injected = 1;
    }
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
