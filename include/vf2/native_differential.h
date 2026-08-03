#ifndef VF2_NATIVE_DIFFERENTIAL_H
#define VF2_NATIVE_DIFFERENTIAL_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/snapshot.h"
#include "vf2/native_runtime.h"
#include "vf2/status.h"

typedef struct vf2_native_differential_report {
    uint32_t start_address;
    uint32_t stop_address;
    uint32_t final_reference_address;
    uint32_t final_native_address;
    size_t minimum_blocks;
    size_t blocks_compared;
    uint64_t reference_instructions_executed;
    uint64_t native_recovered_instructions;
    vf2_native_runtime_step_report last_step;
    vf2_i960_snapshot_diff diff;
    int reached_stop;
} vf2_native_differential_report;

/* Execute the recovered native runtime and the reference i960 in lockstep.
 * Both sides must enter with identical architectural and mutable-memory state.
 * For every accepted native block, the reference processor executes exactly
 * the recovered instruction count reported by vf2_native_runtime_step, then a
 * complete CPU and mutable-memory snapshot comparison is performed.
 *
 * The function never falls back to the interpreter on the native side. A
 * native unsupported path, a reference execution failure, a state mismatch or
 * block-budget exhaustion is returned immediately with a partial report.
 * Reaching stop_address before the first block is a successful zero-length
 * run. */
vf2_status vf2_native_differential_run_until(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t stop_address,
    size_t max_blocks,
    vf2_native_differential_report *report
);

/* Same lockstep contract as vf2_native_differential_run_until, but the stop
 * address is accepted only after at least minimum_blocks have been compared.
 * This is required for repeated scheduler cycles whose start and destination
 * are the same task entry. minimum_blocks must not exceed max_blocks. */
vf2_status vf2_native_differential_run_until_after(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t stop_address,
    size_t minimum_blocks,
    size_t max_blocks,
    vf2_native_differential_report *report
);

#endif
