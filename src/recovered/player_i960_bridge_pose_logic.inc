static vf2_status player_pose_run_original_until(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t stop_address
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status = VF2_OK;

    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.stop_address = stop_address;
    options.max_steps = UINT64_C(2000000);
    options.stop_on_self_branch = true;

    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    return result.halt_reason == VF2_I960_HALT_STOP_ADDRESS &&
           cpu->ip == stop_address
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

/*
 * Live hybrid decompilation of the observed 0x16504 pose corridor.
 *
 * The old bridge reproduced this function by replaying a compressed copro
 * operation stream plus final RAM patches. This replacement executes the
 * still-unrecovered preparation regions with the architectural i960 executor,
 * but substitutes the sixteen repeated joint tails and their table-driven
 * 0x176a0 calls with recovered C against the same live machine state.
 *
 * Consequently every hardware read/write occurs in program order and later C
 * blocks consume effects produced by the earlier original/C blocks. The
 * post-state CRC and architectural register checks remain the hard acceptance
 * gate while the remaining preparation regions are lifted incrementally.
 */
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

        status = player_pose_run_original_until(
            machine, cpu, player_pose_joint_entries[selector]
        );
        if (status != VF2_OK) {
            return status;
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

    status = player_pose_run_original_until(
        machine, cpu, VF2_PLAYER_POSE_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }

    if (cpu->executed_instructions - start_instructions !=
            VF2_PLAYER_POSE_INSTRUCTIONS ||
        cpu->procedure_calls - start_calls != VF2_PLAYER_POSE_JOINT_COUNT ||
        cpu->procedure_returns - start_returns != VF2_PLAYER_POSE_JOINT_COUNT) {
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

vf2_status vf2_hybrid_i960_run_tail(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    const vf2_i960_run_options *options,
    vf2_i960_run_result *result
)
{
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;

    /* 0x16504 -> 0x14418 is an owned recovered boundary. Once a caller asks
     * for this exact corridor there is deliberately no silent ROM fallback:
     * entry guard, live hybrid execution, instruction accounting, CRCs and
     * final architectural state must all prove the recovery. */
    if (cpu != NULL && machine != NULL && options != NULL &&
        cpu->ip == VF2_PLAYER_POSE_ENTRY &&
        options->stop_address == VF2_PLAYER_POSE_RETURN &&
        options->trace_callback == NULL) {
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
        if (result != NULL) {
            memset(result, 0, sizeof(*result));
            result->halt_reason = VF2_I960_HALT_STOP_ADDRESS;
            result->status = VF2_OK;
            result->halt_address = cpu->ip;
            result->executed_instructions =
                cpu->executed_instructions - start_count;
        }
        return VF2_OK;
    }
    return vf2_hybrid_i960_run_tail_previous(cpu, machine, options, result);
}
