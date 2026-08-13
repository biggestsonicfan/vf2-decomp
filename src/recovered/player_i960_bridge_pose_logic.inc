#include "player_i960_bridge_pose_prepare.inc"
#include "player_i960_bridge_pose_diagnostics.inc"
#include "player_i960_bridge_pose_zero.inc"
#include "player_i960_bridge_pose_ten.inc"
#include "player_i960_bridge_pose_final.inc"

/* Fully recovered observed 0x16504 pose corridor. */
static vf2_status player_pose_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint64_t start_instructions = cpu != NULL
        ? cpu->executed_instructions : 0u;
    const uint64_t start_calls = cpu != NULL
        ? cpu->procedure_calls : 0u;
    const uint64_t start_returns = cpu != NULL
        ? cpu->procedure_returns : 0u;
    uint32_t selector = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || !player_pose_entry_matches(machine, cpu)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (selector = 0u; selector < VF2_PLAYER_POSE_JOINT_COUNT; ++selector) {
        uint32_t recovered_selector = UINT32_MAX;
        const vf2_player_pose_prepare_descriptor *preparation =
            player_pose_prepare_descriptor_for(selector, cpu->ip);
        const vf2_player_pose_diag_descriptor *diagnostics =
            player_pose_diag_descriptor_for(selector, cpu->ip);

        if (selector == 0u && cpu->ip == VF2_PLAYER_POSE_ZERO_ENTRY) {
            status = player_pose_execute_zero(machine, cpu);
        } else if (selector == 10u && cpu->ip == VF2_PLAYER_POSE_TEN_ENTRY) {
            status = player_pose_execute_ten(machine, cpu);
        } else if (preparation != NULL) {
            status = player_pose_execute_preparation(machine, cpu, selector);
        } else if (diagnostics != NULL) {
            status = player_pose_execute_diagnostics(machine, cpu, selector);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status != VF2_OK ||
            cpu->ip != player_pose_joint_entries[selector]) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (!player_pose_joint_selector(
                cpu->ip,
                player_pose_joint_returns[selector],
                &recovered_selector) ||
            recovered_selector != selector) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = player_pose_execute_joint(machine, cpu, selector, 0u);
        if (status != VF2_OK ||
            cpu->ip != player_pose_joint_returns[selector]) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }

    if (cpu->ip != VF2_PLAYER_POSE_FINAL_ENTRY) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = player_pose_execute_final(machine, cpu, 0u);
    if (status != VF2_OK) {
        return status;
    }

    if (cpu->executed_instructions - start_instructions !=
            VF2_PLAYER_POSE_INSTRUCTIONS ||
        cpu->procedure_calls - start_calls != VF2_PLAYER_POSE_JOINT_COUNT ||
        cpu->procedure_returns - start_returns !=
            VF2_PLAYER_POSE_JOINT_COUNT + 1u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (!player_pose_crc_matches(
            machine,
            UINT32_C(0xae84ffcb), UINT32_C(0xe8ba2142),
            UINT32_C(0xc4116ecd), UINT32_C(0x2ab1791e))) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (cpu->ip != VF2_PLAYER_POSE_RETURN ||
        cpu->local_frame_depth != 2u ||
        cpu->process_control != UINT32_C(0x001f0000) ||
        cpu->arithmetic_control != UINT32_C(0x3f001002) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL ||
        memcmp(cpu->registers, player_pose_final_registers,
               sizeof(player_pose_final_registers)) != 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return VF2_OK;
}

static void player_pose_set_run_result(
    vf2_i960_run_result *result,
    const vf2_i960_cpu *cpu,
    uint64_t start_count,
    vf2_i960_halt_reason halt_reason,
    vf2_status status
)
{
    if (result != NULL && cpu != NULL) {
        memset(result, 0, sizeof(*result));
        result->halt_reason = halt_reason;
        result->status = status;
        result->halt_address = cpu->ip;
        result->executed_instructions = cpu->executed_instructions - start_count;
    }
}

vf2_status vf2_hybrid_i960_run_tail(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;

    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_POSE_ENTRY &&
        options->trace_callback == NULL) {
        vf2_i960_run_options continuation_options;
        vf2_i960_run_result continuation_result;
        vf2_status status = VF2_OK;

        if (options->max_steps != 0u &&
            options->max_steps < VF2_PLAYER_POSE_INSTRUCTIONS) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (!player_pose_entry_matches(machine, cpu)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = player_pose_execute(machine, cpu);
        if (status != VF2_OK) {
            return status;
        }

        if (options->stop_address == VF2_PLAYER_POSE_RETURN) {
            player_pose_set_run_result(
                result, cpu, start_count,
                VF2_I960_HALT_STOP_ADDRESS, VF2_OK
            );
            return VF2_OK;
        }

        if (options->max_steps == VF2_PLAYER_POSE_INSTRUCTIONS) {
            player_pose_set_run_result(
                result, cpu, start_count,
                VF2_I960_HALT_MAX_STEPS, VF2_OK
            );
            return VF2_OK;
        }

        continuation_options = *options;
        if (continuation_options.max_steps != 0u) {
            continuation_options.max_steps -= VF2_PLAYER_POSE_INSTRUCTIONS;
        }
        memset(&continuation_result, 0, sizeof(continuation_result));
        status = vf2_hybrid_i960_run_tail_previous(
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
    return vf2_hybrid_i960_run_tail_previous(cpu, machine, options, result);
}
