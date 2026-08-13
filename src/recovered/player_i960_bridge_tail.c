/* Earlier sixth-dispatch corridors and the original player bridge. */
#define vf2_hybrid_i960_run_tail vf2_hybrid_i960_run_tail_base
#include "player_i960_bridge_tail_previous.inc"
#undef vf2_hybrid_i960_run_tail

/* Generalized table/state publisher: 0x4b640 -> 0x14414. */
#include "player_i960_bridge_4b640.inc"

/* Semantic player record-stream parser: 0x28178 -> 0x14400. */
#include "player_i960_bridge_28178.inc"

/* Recovered table-driven pose RPC helper: 0x176a0 -> dynamic caller return. */
#include "player_i960_bridge_176a0.inc"

/* Sixteen unrolled joint tails all share one semantic C implementation. */
#include "player_i960_bridge_pose_joint.inc"

/* Generalized non-TGP control-flow family: 0x17710 -> 0x14404. */
#define VF2_PLAYER_17710_ENTRY UINT32_C(0x00017710)
#define VF2_PLAYER_17710_RET UINT32_C(0x00017918)
#define VF2_PLAYER_17710_STOP UINT32_C(0x00014404)

static void player_17710_compat_set_compare_result(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t condition_bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        condition_bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        condition_bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        condition_bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
}

/* Compatibility hook retained by the generic include's legacy macro surface. */
static vf2_status player_execute_17710_fast_exit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    (void)machine;
    (void)cpu;
    return VF2_ERROR_UNSUPPORTED;
}

#define player_read_u8 player_tail_read_u8
#define player_read_u16 player_tail_read_u16
#define player_write_u16 player_tail_write_u16
#define player_set_compare_result player_17710_compat_set_compare_result
#include "player_i960_bridge_17710.inc"
#undef player_execute_17710_fast_exit
#undef player_set_compare_result
#undef player_write_u16
#undef player_read_u16
#undef player_read_u8

/* Full semantic TGP planar-rotation tail, including nonzero angles. */
#include "player_i960_bridge_17710_rotation.inc"

static void player_bridge_result(
    vf2_i960_run_result *result,
    const vf2_i960_cpu *cpu,
    uint64_t start_count
)
{
    if (result != NULL && cpu != NULL) {
        memset(result, 0, sizeof(*result));
        result->halt_reason = VF2_I960_HALT_STOP_ADDRESS;
        result->status = VF2_OK;
        result->halt_address = cpu->ip;
        result->executed_instructions =
            cpu->executed_instructions - start_count;
    }
}

static vf2_status vf2_hybrid_i960_run_tail_4b640(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_4B640_ENTRY &&
        options->trace_callback == NULL &&
        options->stop_address != VF2_PLAYER_4B640_RETURN) {
        vf2_i960_run_options continuation_options;
        vf2_i960_run_result continuation_result;
        uint64_t recovered_count = 0u;
        vf2_status status = player_execute_4b640_general(
            machine, cpu, options->max_steps, &recovered_count
        );

        if (status == VF2_OK && options->stop_address == VF2_PLAYER_4B640_STOP) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_OK) {
            if (status != VF2_ERROR_UNSUPPORTED) {
                return status;
            }
            return vf2_hybrid_i960_run_tail_base(
                cpu, machine, options, result
            );
        }
        if (options->max_steps == recovered_count) {
            if (result != NULL) {
                memset(result, 0, sizeof(*result));
                result->halt_reason = VF2_I960_HALT_MAX_STEPS;
                result->status = VF2_OK;
                result->halt_address = cpu->ip;
                result->executed_instructions = recovered_count;
            }
            return VF2_OK;
        }

        continuation_options = *options;
        if (continuation_options.max_steps != 0u) {
            continuation_options.max_steps -= recovered_count;
        }
        memset(&continuation_result, 0, sizeof(continuation_result));
        status = vf2_hybrid_i960_run_tail_base(
            cpu, machine, &continuation_options, &continuation_result
        );
        if (result != NULL) {
            *result = continuation_result;
            result->status = status;
            result->halt_address = cpu->ip;
            result->executed_instructions =
                cpu->executed_instructions - start_count;
        }
        return status;
    }
    return vf2_hybrid_i960_run_tail_base(cpu, machine, options, result);
}

static vf2_status vf2_hybrid_i960_run_tail_28178(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_28178_STREAM_ENTRY &&
        options->trace_callback == NULL &&
        options->stop_address != VF2_PLAYER_28178_STREAM_FIRST_RETURN) {
        vf2_i960_run_options continuation_options;
        vf2_i960_run_result continuation_result;
        uint64_t recovered_count = 0u;
        vf2_status status = player_execute_28178_stream(
            machine, cpu, options->max_steps, &recovered_count
        );

        if (status == VF2_OK &&
            options->stop_address == VF2_PLAYER_28178_STREAM_STOP) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_OK) {
            if (status != VF2_ERROR_UNSUPPORTED) {
                return status;
            }
            return vf2_hybrid_i960_run_tail_4b640(
                cpu, machine, options, result
            );
        }
        if (options->max_steps == recovered_count) {
            if (result != NULL) {
                memset(result, 0, sizeof(*result));
                result->halt_reason = VF2_I960_HALT_MAX_STEPS;
                result->status = VF2_OK;
                result->halt_address = cpu->ip;
                result->executed_instructions = recovered_count;
            }
            return VF2_OK;
        }

        continuation_options = *options;
        if (continuation_options.max_steps != 0u) {
            continuation_options.max_steps -= recovered_count;
        }
        memset(&continuation_result, 0, sizeof(continuation_result));
        status = vf2_hybrid_i960_run_tail_4b640(
            cpu, machine, &continuation_options, &continuation_result
        );
        if (result != NULL) {
            *result = continuation_result;
            result->status = status;
            result->halt_address = cpu->ip;
            result->executed_instructions =
                cpu->executed_instructions - start_count;
        }
        return status;
    }
    return vf2_hybrid_i960_run_tail_4b640(cpu, machine, options, result);
}

static vf2_status vf2_hybrid_i960_run_tail_176a0(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;
    vf2_status status = VF2_ERROR_UNSUPPORTED;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_176A0_ENTRY &&
        options->trace_callback == NULL &&
        cpu->local_frame_depth != 0u &&
        options->stop_address ==
            cpu->local_frames[cpu->local_frame_depth - 1u].registers[2]) {
        status = player_execute_176a0(machine, cpu, options->max_steps);
        if (status == VF2_OK) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }
    return vf2_hybrid_i960_run_tail_28178(cpu, machine, options, result);
}

static vf2_status vf2_hybrid_i960_run_tail_17710(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;
    vf2_status status = VF2_ERROR_UNSUPPORTED;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_17710_ENTRY &&
        options->stop_address == VF2_PLAYER_17710_STOP &&
        options->trace_callback == NULL) {
        /* Scalar/early-return paths stay fully C. If the planner identifies a
         * true 0x178bc fall-through, probe the read-only prefix and replace
         * only the TGP rotation tail with the recovered semantic matrix. */
        status = player_execute_17710(machine, cpu, options->max_steps);
        if (status == VF2_OK) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
        status = player_execute_17710_rotation(
            machine, cpu, options->max_steps
        );
        if (status == VF2_OK) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }
    return vf2_hybrid_i960_run_tail_176a0(cpu, machine, options, result);
}

/* Full scalar motion integration family: 0x1791c -> 0x14408. */
#include "player_i960_bridge_1791c.inc"

static vf2_status vf2_hybrid_i960_run_tail_1791c(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;
    vf2_status status = VF2_ERROR_UNSUPPORTED;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_1791C_ENTRY &&
        options->stop_address == VF2_PLAYER_1791C_STOP &&
        options->trace_callback == NULL) {
        status = player_execute_1791c(machine, cpu, options->max_steps);
        if (status == VF2_OK) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }
    return vf2_hybrid_i960_run_tail_17710(cpu, machine, options, result);
}

/* Fully recovered observed player pose corridor: 0x16504 -> 0x14418. */
#include "player_i960_bridge_pose_data.inc"
#define vf2_hybrid_i960_run_tail_previous vf2_hybrid_i960_run_tail_1791c
#define vf2_hybrid_i960_run_tail vf2_hybrid_i960_run_tail_pose
#include "player_i960_bridge_pose_logic.inc"
#undef vf2_hybrid_i960_run_tail
#undef vf2_hybrid_i960_run_tail_previous

/* Complete player state update immediately following the pose corridor. */
#include "player_i960_bridge_status_logic.inc"
