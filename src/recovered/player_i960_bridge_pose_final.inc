#define VF2_PLAYER_POSE_FINAL_ENTRY UINT32_C(0x000175f4)
#define VF2_PLAYER_POSE_FINAL_RET UINT32_C(0x0001769c)
#define VF2_PLAYER_POSE_FINAL_COMMAND UINT32_C(0x01000202)
#define VF2_PLAYER_POSE_FINAL_LENGTH_COMMAND UINT32_C(0x16802d2d)
#define VF2_PLAYER_POSE_FINAL_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_PLAYER_POSE_FINAL_MODE0 UINT32_C(0x0050002b)
#define VF2_PLAYER_POSE_FINAL_MODE1 UINT32_C(0x00500031)
#define VF2_PLAYER_POSE_FINAL_THRESHOLD UINT32_C(0x3f333333)

typedef struct vf2_player_pose_final_plan {
    uint32_t player;
    uint32_t port;
    uint32_t runtime_flags;
    uint32_t player_flags;
    uint32_t player_state;
    uint32_t current_pose[3];
    uint32_t previous_pose[3];
    uint32_t delta[3];
    uint32_t planar_x;
    uint32_t planar_z;
    uint8_t mode0;
    uint8_t mode1;
    uint8_t runtime_fast_exit;
    uint8_t reaches_length_command;
} vf2_player_pose_final_plan;

static float player_pose_final_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t player_pose_final_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void player_pose_final_set_compare(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result compare
)
{
    uint32_t bits = 0u;

    if (compare == VF2_I960_COMPARE_LESS) {
        bits = UINT32_C(4);
    } else if (compare == VF2_I960_COMPARE_EQUAL) {
        bits = UINT32_C(2);
    } else if (compare == VF2_I960_COMPARE_GREATER) {
        bits = UINT32_C(1);
    }
    cpu->compare_result = compare;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

static vf2_status player_pose_prepare_final(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps,
    vf2_player_pose_final_plan *plan
)
{
    uint32_t ignored = 0u;
    float current_x = 0.0f;
    float current_z = 0.0f;
    float previous_x = 0.0f;
    float previous_z = 0.0f;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_POSE_FINAL_ENTRY ||
        cpu->local_frame_depth == 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_POSE_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    plan->port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                 cpu->registers[VF2_I960_G0_REGISTER + 12u];
    if (plan->player == 0u || plan->port == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_PLAYER_POSE_FINAL_RUNTIME_FLAGS, &plan->runtime_flags
    );
    if (status != VF2_OK) {
        return status;
    }
    if ((plan->runtime_flags & (UINT32_C(1) << 5u)) != 0u) {
        plan->runtime_fast_exit = 1u;
        if (max_steps != 0u && max_steps < UINT64_C(5)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return VF2_OK;
    }

    for (index = 0u; index < 3u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            plan->player + UINT32_C(0x1f4) +
                (uint32_t)index * UINT32_C(4),
            &plan->current_pose[index]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                plan->player + UINT32_C(0x6e0) +
                    (uint32_t)index * UINT32_C(4),
                &plan->previous_pose[index]
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                plan->player + UINT32_C(0x1e00) +
                    (uint32_t)index * UINT32_C(4),
                &plan->delta[index]
            );
        }
        if (status == VF2_OK) {
            /* Bounds-preflight the previous-pose snapshot destination. */
            status = vf2_model2a_read_u32(
                machine,
                plan->player + UINT32_C(0x1e0c) +
                    (uint32_t)index * UINT32_C(4),
                &ignored
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    status = vf2_model2a_read(
        machine, VF2_PLAYER_POSE_FINAL_MODE0,
        &plan->mode0, sizeof(plan->mode0)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_PLAYER_POSE_FINAL_MODE1,
            &plan->mode1, sizeof(plan->mode1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, plan->player, &plan->player_flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    current_x = player_pose_final_float(plan->current_pose[0]);
    current_z = player_pose_final_float(plan->current_pose[2]);
    previous_x = player_pose_final_float(plan->previous_pose[0]);
    previous_z = player_pose_final_float(plan->previous_pose[2]);
    if (!isfinite(current_x) || !isfinite(current_z) ||
        !isfinite(previous_x) || !isfinite(previous_z)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    plan->planar_x = player_pose_final_bits(current_x - previous_x);
    plan->planar_z = player_pose_final_bits(current_z - previous_z);

    plan->reaches_length_command =
        plan->mode0 == UINT8_C(9) &&
        plan->mode1 == UINT8_C(9) &&
        (plan->player_flags & (UINT32_C(1) << 7u)) != 0u;
    if (plan->reaches_length_command != 0u) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x1a4), &plan->player_state
        );
        if (status != VF2_OK) {
            return status;
        }
        /* The longest state-bit fall-through is 33 instructions including
         * RET. Reject an insufficient budget before the first visible write. */
        if (max_steps != 0u && max_steps < UINT64_C(33)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    return VF2_OK;
}

static vf2_status player_pose_apply_final(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_pose_final_plan *plan
)
{
    uint64_t instruction_count = UINT64_C(4);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_POSE_FINAL_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[15] = VF2_PLAYER_POSE_FINAL_COMMAND;
    status = vf2_model2a_write_u32(
        machine, plan->port, VF2_PLAYER_POSE_FINAL_COMMAND
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = plan->runtime_flags;

    if (plan->runtime_fast_exit == 0u) {
        size_t index = 0u;

        cpu->registers[8] = plan->current_pose[0];
        cpu->registers[9] = plan->current_pose[1];
        cpu->registers[10] = plan->current_pose[2];
        cpu->registers[12] = plan->previous_pose[0];
        cpu->registers[13] = plan->previous_pose[1];
        cpu->registers[14] = plan->previous_pose[2];
        for (index = 0u; index < 3u; ++index) {
            status = vf2_model2a_write_u32(
                machine,
                plan->player + UINT32_C(0x1e0c) +
                    (uint32_t)index * UINT32_C(4),
                plan->previous_pose[index]
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine,
                    plan->player + UINT32_C(0x6e0) +
                        (uint32_t)index * UINT32_C(4),
                    plan->current_pose[index]
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        cpu->registers[8] = plan->planar_x;
        cpu->registers[10] = plan->planar_z;
        cpu->registers[4] = plan->delta[0];
        cpu->registers[5] = plan->delta[1];
        cpu->registers[6] = plan->delta[2];
        cpu->registers[3] = UINT32_C(9);
        cpu->registers[14] = plan->mode0;
        instruction_count = UINT64_C(14);

        if (plan->mode0 == UINT8_C(9)) {
            cpu->registers[3] = UINT32_C(9);
            cpu->registers[14] = plan->mode1;
            instruction_count += UINT64_C(3);
            if (plan->mode1 == UINT8_C(9)) {
                cpu->registers[15] = plan->player_flags;
                instruction_count += UINT64_C(2);
                if ((plan->player_flags & (UINT32_C(1) << 7u)) != 0u) {
                    uint32_t length_bits = 0u;
                    const float threshold =
                        player_pose_final_float(VF2_PLAYER_POSE_FINAL_THRESHOLD);
                    float length = 0.0f;
                    vf2_i960_compare_result compare = VF2_I960_COMPARE_EQUAL;

                    cpu->registers[15] = VF2_PLAYER_POSE_FINAL_LENGTH_COMMAND;
                    status = vf2_model2a_write_u32(
                        machine, plan->port,
                        VF2_PLAYER_POSE_FINAL_LENGTH_COMMAND
                    );
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, plan->port, plan->planar_x
                        );
                    }
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, plan->port, plan->planar_z
                        );
                    }
                    if (status == VF2_OK) {
                        status = vf2_model2a_read_u32(
                            machine, plan->port, &length_bits
                        );
                    }
                    if (status != VF2_OK) {
                        return status;
                    }
                    cpu->registers[4] = length_bits;
                    cpu->registers[3] = VF2_PLAYER_POSE_FINAL_THRESHOLD;
                    length = player_pose_final_float(length_bits);
                    if (length < threshold) {
                        compare = VF2_I960_COMPARE_LESS;
                    } else if (length > threshold) {
                        compare = VF2_I960_COMPARE_GREATER;
                    }
                    player_pose_final_set_compare(cpu, compare);
                    instruction_count += UINT64_C(8);

                    if (compare != VF2_I960_COMPARE_LESS) {
                        cpu->registers[3] = plan->player_state;
                        instruction_count += UINT64_C(2);
                        if ((plan->player_state & (UINT32_C(1) << 14u)) == 0u) {
                            ++instruction_count;
                            if ((plan->player_state &
                                 (UINT32_C(1) << 17u)) == 0u) {
                                ++instruction_count;
                                if ((plan->player_state &
                                     (UINT32_C(1) << 18u)) == 0u) {
                                    ++instruction_count;
                                    /* Bit 19 is tested regardless of value;
                                     * both outcomes converge at RET. */
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    cpu->ip = VF2_PLAYER_POSE_FINAL_RET;
    cpu->executed_instructions += instruction_count;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_POSE_RETURN
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_pose_execute_final(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    vf2_player_pose_final_plan plan;
    vf2_status status = player_pose_prepare_final(
        machine, cpu, max_steps, &plan
    );

    if (status != VF2_OK) {
        return status;
    }
    return player_pose_apply_final(machine, cpu, &plan);
}
