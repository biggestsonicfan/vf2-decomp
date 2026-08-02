#include "vf2/native_runtime.h"

#include <string.h>

#define VF2_NATIVE_FRAME_WAIT_POLL_ENTRY UINT32_C(0x00010f90)
#define VF2_NATIVE_INTERRUPT_RETURN_ENTRY UINT32_C(0x00000d20)
#define VF2_NATIVE_SECOND_SCHEDULER_ENTRY UINT32_C(0x0000a010)

static void accumulate_step(
    vf2_native_runtime_state *state,
    const vf2_native_runtime_step_report *report
)
{
    ++state->blocks_executed;
    state->recovered_instruction_count += report->recovered_instruction_count;
    state->recovered_procedure_calls += report->recovered_procedure_calls;
    state->recovered_procedure_returns += report->recovered_procedure_returns;

    if (report->kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
        ++state->frame_wait_phases;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER) {
        ++state->scheduler_entries;
    }
}

vf2_status vf2_native_runtime_initialize(
    vf2_native_runtime_state *state,
    size_t frame_wait_visits_before_interrupt
)
{
    vf2_status status = VF2_OK;

    if (state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    status = vf2_hybrid_frame_wait_initialize(
        &state->frame_wait,
        frame_wait_visits_before_interrupt
    );
    if (status != VF2_OK) {
        memset(state, 0, sizeof(*state));
    }
    return status;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = cpu->ip;

    if (cpu->ip == VF2_NATIVE_FRAME_WAIT_POLL_ENTRY ||
        cpu->ip == VF2_NATIVE_INTERRUPT_RETURN_ENTRY) {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = vf2_hybrid_frame_wait_execute(
            machine,
            cpu,
            &state->frame_wait,
            &bridge_report
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_NATIVE_SECOND_SCHEDULER_ENTRY) {
        vf2_hybrid_second_scheduler_report scheduler_report;
        memset(&scheduler_report, 0, sizeof(scheduler_report));
        status = vf2_hybrid_second_scheduler_enter(
            machine,
            cpu,
            &scheduler_report
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER;
            local_report.bridge_kind =
                VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                scheduler_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                scheduler_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                scheduler_report.recovered_procedure_returns;
        }
    } else {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = vf2_hybrid_post_frame_bridge_execute(
            machine,
            cpu,
            &bridge_report
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    }

    if (status == VF2_OK) {
        accumulate_step(state, &local_report);
    }
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_native_runtime_run_until(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    uint32_t stop_address,
    size_t max_blocks,
    vf2_native_runtime_run_report *report
)
{
    vf2_native_runtime_run_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = cpu->ip;
    local_report.stop_address = stop_address;
    local_report.final_address = cpu->ip;

    while (cpu->ip != stop_address && local_report.blocks_executed < max_blocks) {
        vf2_native_runtime_step_report step_report;
        memset(&step_report, 0, sizeof(step_report));
        status = vf2_native_runtime_step(
            machine,
            cpu,
            state,
            &step_report
        );
        if (status != VF2_OK) {
            break;
        }
        ++local_report.blocks_executed;
        local_report.recovered_instruction_count +=
            step_report.recovered_instruction_count;
        local_report.recovered_procedure_calls +=
            step_report.recovered_procedure_calls;
        local_report.recovered_procedure_returns +=
            step_report.recovered_procedure_returns;
        if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
            ++local_report.frame_wait_phases;
        } else if (
            step_report.kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER
        ) {
            ++local_report.scheduler_entries;
        }
        local_report.last_step_kind = step_report.kind;
        local_report.last_bridge_kind = step_report.bridge_kind;
        local_report.final_address = cpu->ip;
    }

    if (status == VF2_OK && cpu->ip == stop_address) {
        local_report.reached_stop = 1;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    local_report.final_address = cpu->ip;
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

const char *vf2_native_runtime_step_kind_name(
    vf2_native_runtime_step_kind kind
)
{
    switch (kind) {
    case VF2_NATIVE_RUNTIME_STEP_NONE:
        return "none";
    case VF2_NATIVE_RUNTIME_STEP_BRIDGE:
        return "bridge";
    case VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT:
        return "frame-wait";
    case VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER:
        return "second-scheduler";
    default:
        return "unknown";
    }
}
