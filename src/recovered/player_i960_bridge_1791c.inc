#include <math.h>

#define VF2_PLAYER_1791C_ENTRY UINT32_C(0x0001791c)
#define VF2_PLAYER_1791C_RET UINT32_C(0x00017b64)
#define VF2_PLAYER_1791C_STOP UINT32_C(0x00014408)
#define VF2_PLAYER_1791C_LENGTH_COMMAND UINT32_C(0x16802d2d)

typedef struct vf2_player_1791c_plan {
    uint32_t player;
    uint32_t port;
    uint32_t initial_position[3];
    uint32_t position[3];
    uint32_t velocity[3];
    uint32_t secondary_x;
    uint32_t secondary_z;
    uint32_t secondary_factor;
    uint32_t delta[3];
    uint32_t final_flat_port_value;
    uint64_t instruction_count;
} vf2_player_1791c_plan;

static float player_1791c_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t player_1791c_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/*
 * Command 0x16802d2d is used throughout the player code as a two-component
 * scalar norm.  The 0x1791c caller feeds X/Z velocity pairs, compares the
 * returned scalar against a speed limit, then multiplies both components by
 * the same ratio.  Other call sites use the same two-input/one-output shape
 * for distance thresholds.  Keep the recovered primitive deliberately small:
 * finite IEEE-754 inputs only, with unsupported values delegated to i960/TGP.
 */
static vf2_status player_1791c_length2(
    uint32_t x_bits,
    uint32_t z_bits,
    uint32_t *length_bits
)
{
    const float x = player_1791c_float(x_bits);
    const float z = player_1791c_float(z_bits);
    float length = 0.0f;

    if (length_bits == NULL || !isfinite(x) || !isfinite(z)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    length = sqrtf(x * x + z * z);
    if (!isfinite(length)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *length_bits = player_1791c_bits(length);
    return VF2_OK;
}

static vf2_status player_prepare_1791c(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_1791c_plan *plan
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t player_state = 0u;
    uint32_t player_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t global_tick = 0u;
    uint32_t global_scale = 0u;
    uint32_t global_secondary_scale = 0u;
    uint32_t global_position_threshold = 0u;
    uint32_t global_unused_gate_scale = 0u;
    uint32_t player_speed_scale = 0u;
    uint32_t vertical_bias = 0u;
    uint32_t base_x = 0u;
    uint32_t base_z = 0u;
    uint32_t secondary_y = 0u;
    uint32_t length_bits = 0u;
    uint16_t gate = 0u;
    float position[3];
    float initial[3];
    float velocity[3];
    float secondary_x = 0.0f;
    float secondary_z = 0.0f;
    float secondary_factor = 0.0f;
    float first_limit = 0.0f;
    float second_limit = 0.0f;
    float length = 0.0f;
    float base_x_value = 0.0f;
    float base_z_value = 0.0f;
    float threshold = 0.0f;
    float tick = 0.0f;
    float secondary_y_value = 0.0f;
    bool first_scaled = false;
    bool apply_vertical = false;
    vf2_status status = VF2_OK;
    size_t index = 0u;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_1791C_ENTRY ||
        cpu->local_frame_depth < 3u || player == 0u || port == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_1791C_STOP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = player;
    plan->port = port;

    for (index = 0u; index < 3u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            player + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
            &plan->initial_position[index]
        );
        if (status != VF2_OK) {
            return status;
        }
        initial[index] = player_1791c_float(plan->initial_position[index]);
        position[index] = initial[index];
        if (!isfinite(initial[index])) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    status = vf2_model2a_read_u32(machine, player + UINT32_C(0x2c), &plan->velocity[0]);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x30), &plan->velocity[1]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x34), &plan->velocity[2]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x5c8), &plan->secondary_x);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x5cc), &secondary_y);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x5d0), &plan->secondary_z);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x5d4), &plan->secondary_factor);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x5d8), &player_speed_scale);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x1a4), &player_state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &player_flags);
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(machine, player + UINT32_C(0x61c), &gate);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x1f4), &base_x);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player + UINT32_C(0x1fc), &base_z);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &runtime_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a000), &global_tick);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a004), &global_scale);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a008), &global_secondary_scale);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a00c), &global_position_threshold);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a010), &global_unused_gate_scale);
    }
    if (status != VF2_OK) {
        return status;
    }
    (void)global_unused_gate_scale;

    for (index = 0u; index < 3u; ++index) {
        velocity[index] = player_1791c_float(plan->velocity[index]);
        if (!isfinite(velocity[index])) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    secondary_x = player_1791c_float(plan->secondary_x);
    secondary_z = player_1791c_float(plan->secondary_z);
    secondary_factor = player_1791c_float(plan->secondary_factor);
    secondary_y_value = player_1791c_float(secondary_y);
    base_x_value = player_1791c_float(base_x);
    base_z_value = player_1791c_float(base_z);
    threshold = player_1791c_float(global_position_threshold);
    tick = player_1791c_float(global_tick);
    if (!isfinite(secondary_x) || !isfinite(secondary_z) ||
        !isfinite(secondary_factor) || !isfinite(secondary_y_value) ||
        !isfinite(base_x_value) || !isfinite(base_z_value) ||
        !isfinite(threshold) || !isfinite(tick)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    first_limit = player_1791c_float(global_scale) * tick;
    first_limit *= player_1791c_float(player_speed_scale);
    first_limit *= player_1791c_float(UINT32_C(0x3c888888));
    first_limit *= 1.0f + secondary_factor;
    second_limit = player_1791c_float(global_secondary_scale) * first_limit;
    if (!isfinite(first_limit) || !isfinite(second_limit) ||
        first_limit < 0.0f || second_limit < 0.0f) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* 0x1791c through the r7=0 setup. */
    plan->instruction_count = UINT64_C(16);
    /* 0x17974 ld + 0x17978 bbs. */
    plan->instruction_count += UINT64_C(2);

    if ((player_state & (UINT32_C(1) << 4u)) == 0u) {
        status = player_1791c_length2(
            plan->velocity[0], plan->velocity[2], &length_bits
        );
        if (status != VF2_OK) {
            return status;
        }
        length = player_1791c_float(length_bits);
        /* lda/st command, two ld/st arguments, one result read, compare. */
        plan->instruction_count += UINT64_C(8);
        if (length <= first_limit) {
            velocity[0] = 0.0f;
            velocity[2] = 0.0f;
            plan->instruction_count += UINT64_C(3);
        } else {
            const float scale = 1.0f - first_limit / length;
            if (!isfinite(scale)) {
                return VF2_ERROR_UNSUPPORTED;
            }
            velocity[0] *= scale;
            velocity[2] *= scale;
            first_scaled = true;
            plan->instruction_count += UINT64_C(11);
        }
    }

    plan->final_flat_port_value = plan->secondary_z;
    status = player_1791c_length2(
        plan->secondary_x, plan->secondary_z, &length_bits
    );
    if (status != VF2_OK) {
        return status;
    }
    length = player_1791c_float(length_bits);
    plan->instruction_count += UINT64_C(8);
    if (length <= second_limit) {
        secondary_x = 0.0f;
        secondary_z = 0.0f;
        plan->instruction_count += UINT64_C(4);
        if (!first_scaled) {
            secondary_factor = 0.0f;
            ++plan->instruction_count;
        }
    } else {
        const float scale = 1.0f - second_limit / length;
        if (!isfinite(scale)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        secondary_x *= scale;
        secondary_z *= scale;
        plan->instruction_count += UINT64_C(10);
    }

    /* 0x17a40: choose whether the vertical velocity receives gravity+bias. */
    plan->instruction_count += UINT64_C(2);
    if ((player_flags & (UINT32_C(1) << 2u)) != 0u) {
        apply_vertical = true;
    } else {
        plan->instruction_count += UINT64_C(2);
        if (gate == 0u) {
            plan->instruction_count += UINT64_C(7);
            if (fabsf(base_x_value) >= threshold) {
                ++plan->instruction_count;
                apply_vertical = true;
            } else {
                plan->instruction_count += UINT64_C(2);
                if (fabsf(base_z_value) >= threshold) {
                    ++plan->instruction_count;
                    apply_vertical = true;
                } else {
                    plan->instruction_count += UINT64_C(3);
                }
            }
        }
    }
    if (apply_vertical) {
        velocity[1] = velocity[1] - tick + secondary_y_value;
        plan->instruction_count += UINT64_C(8);
    }

    position[0] += velocity[0];
    position[1] += velocity[1];
    position[2] += velocity[2];
    plan->instruction_count += UINT64_C(12);

    plan->instruction_count += UINT64_C(2);
    if ((runtime_flags & (UINT32_C(1) << 20u)) != 0u) {
        position[0] += 0.5f * secondary_x;
        position[2] += 0.5f * secondary_z;
        plan->instruction_count += UINT64_C(13);
    } else {
        position[0] += secondary_x;
        position[2] += secondary_z;
        plan->instruction_count += UINT64_C(8);
    }

    for (index = 0u; index < 3u; ++index) {
        const float difference = position[index] - initial[index];
        if (!isfinite(position[index]) || !isfinite(difference)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        plan->position[index] = player_1791c_bits(position[index]);
        plan->delta[index] = player_1791c_bits(difference);
    }
    plan->velocity[0] = player_1791c_bits(velocity[0]);
    plan->velocity[1] = player_1791c_bits(velocity[1]);
    plan->velocity[2] = player_1791c_bits(velocity[2]);
    plan->secondary_x = player_1791c_bits(secondary_x);
    plan->secondary_z = player_1791c_bits(secondary_z);
    plan->secondary_factor = player_1791c_bits(secondary_factor);
    /* ldt/ldt, three subr, stt and ret. */
    plan->instruction_count += UINT64_C(7);
    return VF2_OK;
}

static vf2_status player_apply_1791c(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_1791c_plan *plan
)
{
    vf2_status status = VF2_OK;
    size_t index = 0u;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_1791C_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        cpu->local_frame_depth < 3u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_1791C_STOP || plan->instruction_count == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write(
        machine, UINT32_C(0x0050e000),
        plan->initial_position, sizeof(plan->initial_position)
    );
    /* With no real coprocessor callback, preserve the final observable flat
     * port word left by the second length command.  Callback-backed runtimes
     * are semantic replacements and must not receive an unconsumed fake RPC. */
    if (status == VF2_OK &&
        machine->copro_read_callback == NULL &&
        machine->copro_write_callback == NULL) {
        status = vf2_model2a_write_u32(
            machine, plan->port, plan->final_flat_port_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x2c), plan->velocity[0]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x30), plan->velocity[1]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x34), plan->velocity[2]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x5c8), plan->secondary_x
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x5d0), plan->secondary_z
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x5d4), plan->secondary_factor
        );
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(
            machine,
            plan->player + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
            plan->position[index]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, plan->player + UINT32_C(0x1e00),
            plan->delta, sizeof(plan->delta)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = VF2_PLAYER_1791C_RET;
    cpu->executed_instructions += plan->instruction_count - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_1791C_STOP
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_execute_1791c(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    vf2_player_1791c_plan plan;
    vf2_status status = player_prepare_1791c(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    if (max_steps != 0u && max_steps < plan.instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return player_apply_1791c(machine, cpu, &plan);
}
