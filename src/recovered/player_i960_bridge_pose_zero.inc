#include <math.h>

#define VF2_PLAYER_POSE_ZERO_ENTRY UINT32_C(0x00016504)
#define VF2_PLAYER_POSE_ZERO_STOP UINT32_C(0x00016654)
#define VF2_PLAYER_POSE_ZERO_COMMAND_SIDE UINT32_C(0x1c003838)
#define VF2_PLAYER_POSE_ZERO_COMMAND_PREFIX UINT32_C(0x00800101)
#define VF2_PLAYER_POSE_ZERO_COMMAND_MODE UINT32_C(0x01800303)
#define VF2_PLAYER_POSE_ZERO_COMMAND_SETUP UINT32_C(0x07000e0e)
#define VF2_PLAYER_POSE_ZERO_COMMAND_VECTOR UINT32_C(0x03000606)
#define VF2_PLAYER_POSE_ZERO_COMMAND_CAPTURE UINT32_C(0x09001212)
#define VF2_PLAYER_POSE_ZERO_COMMAND_ANGLE UINT32_C(0x34806969)
#define VF2_PLAYER_POSE_ZERO_GLOBAL_CAPTURE UINT32_C(0x00501024)

typedef struct vf2_player_pose_zero_plan {
    uint32_t player;
    uint32_t port;
    uint32_t side;
    uint32_t setup_word;
    uint32_t initial_vector[3];
    uint32_t branch_vector[3];
    uint32_t current_angle;
    uint32_t side_angle;
    uint32_t player_flags;
    uint32_t diagnostic_cursor;
    uint64_t instruction_count;
    uint8_t gate;
    uint8_t write_branch_vector;
} vf2_player_pose_zero_plan;

static float player_pose_zero_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t player_pose_zero_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static vf2_status player_pose_prepare_zero(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_pose_zero_plan *plan
)
{
    uint16_t angle = 0u;
    uint32_t vertical_offset = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_POSE_ZERO_ENTRY) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    plan->port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                 cpu->registers[VF2_I960_G0_REGISTER + 12u];
    if (plan->player == 0u || plan->port == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_tail_read_u8(
        machine, plan->player + UINT32_C(4), &plan->gate
    );
    plan->side = (uint32_t)plan->gate;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x650), &plan->setup_word
        );
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            plan->player + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
            &plan->initial_vector[index]
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, plan->player + UINT32_C(0x26), &angle
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player, &plan->player_flags
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u8(
            machine, plan->player + UINT32_C(0x840), &plan->gate
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_POSE_DIAG_CURSOR, &plan->diagnostic_cursor
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    plan->current_angle = (uint32_t)(int32_t)(int16_t)angle;

    for (index = 0u; index < 9u; ++index) {
        uint32_t ignored = 0u;
        status = vf2_model2a_read_u32(
            machine,
            VF2_PLAYER_POSE_ZERO_GLOBAL_CAPTURE +
                (uint32_t)index * UINT32_C(4),
            &ignored
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    plan->instruction_count = UINT64_C(33);
    if (plan->current_angle != 0u) {
        uint32_t ignored = 0u;
        status = vf2_model2a_read_u32(
            machine,
            VF2_PLAYER_POSE_DIAG_BASE + plan->diagnostic_cursor,
            &ignored
        );
        if (status != VF2_OK) {
            return status;
        }
        plan->instruction_count += UINT64_C(6);
    }

    if ((plan->player_flags & (UINT32_C(1) << 2u)) == 0u) {
        float y = 0.0f;

        for (index = 0u; index < 3u; ++index) {
            status = vf2_model2a_read_u32(
                machine,
                plan->player + UINT32_C(0x80) +
                    (uint32_t)index * UINT32_C(4),
                &plan->branch_vector[index]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0xc50), &vertical_offset
        );
        if (status != VF2_OK) {
            return status;
        }
        y = player_pose_zero_float(plan->branch_vector[1]) +
            player_pose_zero_float(vertical_offset);
        if (!isfinite(y)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        plan->branch_vector[1] = player_pose_zero_bits(y);
        plan->write_branch_vector = 1u;
        plan->instruction_count += UINT64_C(6);
    } else {
        plan->instruction_count += UINT64_C(2);
        if ((plan->gate & UINT8_C(1)) != 0u) {
            status = vf2_model2a_read_u32(
                machine, plan->player + UINT32_C(0x80),
                &plan->branch_vector[0]
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, plan->player + UINT32_C(0x88),
                    &plan->branch_vector[2]
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            plan->branch_vector[1] = 0u;
            plan->write_branch_vector = 1u;
            plan->instruction_count += UINT64_C(9);
        }
    }

    plan->side_angle =
        ((plan->side & UINT32_C(1)) != 0u
            ? UINT32_C(0x3b00) : UINT32_C(0x3a00));
    plan->instruction_count +=
        (plan->side & UINT32_C(1)) != 0u ? UINT64_C(8) : UINT64_C(9);
    return VF2_OK;
}

static vf2_status player_pose_zero_write_triplet(
    vf2_model2a *machine,
    uint32_t port,
    const uint32_t values[3]
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < 3u; ++index) {
        status = vf2_model2a_write_u32(
            machine, port + (uint32_t)index * UINT32_C(4), values[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static vf2_status player_pose_apply_zero(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_pose_zero_plan *plan
)
{
    uint32_t captured[9] = {0u};
    uint32_t cursor = plan != NULL ? plan->diagnostic_cursor : 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_POSE_ZERO_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

#define VF2_POSE_ZERO_WRITE(value_) \
    do { \
        status = vf2_model2a_write_u32(machine, plan->port, (value_)); \
        if (status != VF2_OK) return status; \
    } while (0)

    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_SIDE);
    VF2_POSE_ZERO_WRITE(plan->side);
    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_PREFIX);
    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_MODE);
    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_SETUP);
    VF2_POSE_ZERO_WRITE(0u);
    VF2_POSE_ZERO_WRITE(plan->setup_word);
    VF2_POSE_ZERO_WRITE(0u);
    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_VECTOR);
    status = player_pose_zero_write_triplet(
        machine, plan->port, plan->initial_vector
    );
    if (status != VF2_OK) {
        return status;
    }

    if (plan->current_angle != 0u) {
        status = player_pose_diag_emit(
            machine, plan->port, plan->player, &cursor,
            plan->current_angle, UINT32_C(0x36806d6d)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_CAPTURE);
    for (index = 0u; index < 9u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            plan->port + (uint32_t)(index % 3u) * UINT32_C(4),
            &captured[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    for (index = 0u; index < 9u; ++index) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_PLAYER_POSE_ZERO_GLOBAL_CAPTURE +
                (uint32_t)index * UINT32_C(4),
            captured[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    if (plan->write_branch_vector != 0u) {
        VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_VECTOR);
        status = player_pose_zero_write_triplet(
            machine, plan->port, plan->branch_vector
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    VF2_POSE_ZERO_WRITE(VF2_PLAYER_POSE_ZERO_COMMAND_ANGLE);
    VF2_POSE_ZERO_WRITE(plan->side_angle);
    VF2_POSE_ZERO_WRITE(plan->current_angle);
#undef VF2_POSE_ZERO_WRITE

    cpu->executed_instructions += plan->instruction_count;
    cpu->ip = VF2_PLAYER_POSE_ZERO_STOP;
    return VF2_OK;
}

static vf2_status player_pose_execute_zero(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_player_pose_zero_plan plan;
    vf2_status status = player_pose_prepare_zero(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    return player_pose_apply_zero(machine, cpu, &plan);
}
