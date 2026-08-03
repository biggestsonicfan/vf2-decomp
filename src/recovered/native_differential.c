#include "vf2/native_differential.h"

#include <string.h>

static vf2_status compare_complete_state(
    const vf2_model2a *reference_machine,
    const vf2_i960_cpu *reference_cpu,
    const vf2_model2a *native_machine,
    const vf2_i960_cpu *native_cpu,
    vf2_i960_snapshot *reference_snapshot,
    vf2_i960_snapshot *native_snapshot,
    vf2_i960_snapshot_diff *diff
)
{
    vf2_status status = vf2_i960_snapshot_capture(
        reference_snapshot,
        reference_cpu,
        reference_machine
    );
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            native_snapshot,
            native_cpu,
            native_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(
            reference_snapshot,
            native_snapshot,
            diff
        );
    }
    return status;
}

static void record_initial_ip_mismatch(
    vf2_native_differential_report *report,
    uint32_t reference_ip,
    uint32_t native_ip
)
{
    report->diff.equal = false;
    (void)strcpy(report->diff.component, "cpu-ip");
    report->diff.differing_bytes = sizeof(reference_ip);
    report->diff.first_offset = 0u;
    report->diff.expected_value = reference_ip;
    report->diff.actual_value = native_ip;
}

vf2_status vf2_native_differential_run_until(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t stop_address,
    size_t max_blocks,
    vf2_native_differential_report *report
)
{
    vf2_native_differential_report local_report;
    vf2_i960_snapshot reference_snapshot;
    vf2_i960_snapshot native_snapshot;
    vf2_status status = VF2_OK;

    if (reference_machine == NULL || reference_cpu == NULL ||
        native_machine == NULL || native_cpu == NULL ||
        native_state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = native_cpu->ip;
    local_report.stop_address = stop_address;
    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    local_report.diff.equal = true;

    vf2_i960_snapshot_init(&reference_snapshot);
    vf2_i960_snapshot_init(&native_snapshot);

    if (reference_cpu->ip != native_cpu->ip) {
        record_initial_ip_mismatch(
            &local_report,
            reference_cpu->ip,
            native_cpu->ip
        );
        status = VF2_ERROR_UNSUPPORTED;
    }

    while (status == VF2_OK &&
           native_cpu->ip != stop_address &&
           local_report.blocks_compared < max_blocks) {
        vf2_native_runtime_step_report step_report;
        const uint64_t reference_instruction_start =
            reference_cpu->executed_instructions;
        uint64_t reference_step = 0u;

        if (reference_cpu->ip != native_cpu->ip) {
            record_initial_ip_mismatch(
                &local_report,
                reference_cpu->ip,
                native_cpu->ip
            );
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        memset(&step_report, 0, sizeof(step_report));
        status = vf2_native_runtime_step(
            native_machine,
            native_cpu,
            native_state,
            &step_report
        );
        local_report.last_step = step_report;
        if (status != VF2_OK) {
            break;
        }
        if (step_report.recovered_instruction_count == 0u) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        for (reference_step = 0u;
             status == VF2_OK &&
             reference_step < step_report.recovered_instruction_count;
             ++reference_step) {
            status = vf2_i960_step(
                reference_cpu,
                reference_machine,
                NULL
            );
        }

        local_report.reference_instructions_executed +=
            reference_cpu->executed_instructions -
            reference_instruction_start;
        local_report.native_recovered_instructions +=
            step_report.recovered_instruction_count;

        if (status != VF2_OK) {
            break;
        }
        if (reference_cpu->executed_instructions -
                reference_instruction_start !=
            step_report.recovered_instruction_count) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        memset(&local_report.diff, 0, sizeof(local_report.diff));
        status = compare_complete_state(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            &reference_snapshot,
            &native_snapshot,
            &local_report.diff
        );
        if (status != VF2_OK) {
            break;
        }
        if (!local_report.diff.equal) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        ++local_report.blocks_compared;
    }

    if (status == VF2_OK &&
        reference_cpu->ip == stop_address &&
        native_cpu->ip == stop_address) {
        local_report.reached_stop = 1;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    if (report != NULL) {
        *report = local_report;
    }

    vf2_i960_snapshot_destroy(&reference_snapshot);
    vf2_i960_snapshot_destroy(&native_snapshot);
    return status;
}
