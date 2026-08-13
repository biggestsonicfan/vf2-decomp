/* Earlier sixth-dispatch corridors: 0x4b640 and the original player bridge. */
#define vf2_hybrid_i960_run_tail vf2_hybrid_i960_run_tail_base
#include "player_i960_bridge_tail_previous.inc"
#undef vf2_hybrid_i960_run_tail

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

/* The generalized include was first written as a drop-in replacement for the
 * exact helper in player_i960_bridge.c.  Keep that compatibility reference
 * satisfied in this independent dispatch layer. */
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

/* TGP-facing subset whose rotation angle is proven to be exactly zero. */
#include "player_i960_bridge_17710_rotation0.inc"

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
        status = player_execute_17710_rotation_zero(
            machine, cpu, options->max_steps
        );
        if (status == VF2_OK) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
        status = player_execute_17710(machine, cpu, options->max_steps);
        if (status == VF2_OK) {
            player_bridge_result(result, cpu, start_count);
            return VF2_OK;
        }
        if (status != VF2_ERROR_UNSUPPORTED) {
            return status;
        }
    }
    return vf2_hybrid_i960_run_tail_base(cpu, machine, options, result);
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

/* Observed sixth-dispatch player pose corridor: 0x16504 -> 0x14418. */
#include "player_i960_bridge_pose_data.inc"
#define vf2_hybrid_i960_run_tail_previous vf2_hybrid_i960_run_tail_1791c
#define vf2_hybrid_i960_run_tail vf2_hybrid_i960_run_tail_pose
#include "player_i960_bridge_pose_logic.inc"
#undef vf2_hybrid_i960_run_tail
#undef vf2_hybrid_i960_run_tail_previous

/* Complete player state update immediately following the pose corridor. */
#include "player_i960_bridge_status_logic.inc"
