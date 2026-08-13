#define VF2_PLAYER_POSE_DIAG_VECTOR_COMMAND UINT32_C(0x03000606)
#define VF2_PLAYER_POSE_DIAG_ANGLE_COMMAND UINT32_C(0x34806969)
#define VF2_PLAYER_POSE_DIAG_TRIPLET_COMMAND UINT32_C(0x1f803f3f)
#define VF2_PLAYER_POSE_DIAG_GLOBAL_COMMAND UINT32_C(0x08801111)
#define VF2_PLAYER_POSE_DIAG_PREFIX UINT32_C(0x00800101)
#define VF2_PLAYER_POSE_DIAG_PREFIX2 UINT32_C(0x01000202)
#define VF2_PLAYER_POSE_DIAG_CURSOR UINT32_C(0x005001e4)
#define VF2_PLAYER_POSE_DIAG_BASE UINT32_C(0x0090e000)
#define VF2_PLAYER_POSE_DIAG_GLOBAL_TRIPLES UINT32_C(0x00501024)
#define VF2_PLAYER_POSE_DIAG_MAX_FIELDS 6u

typedef struct vf2_player_pose_diag_field {
    uint16_t offset;
    uint32_t command;
} vf2_player_pose_diag_field;

typedef struct vf2_player_pose_diag_descriptor {
    uint32_t selector;
    uint32_t entry;
    uint32_t stop;
    uint64_t base_instruction_count_side0;
    uint64_t base_instruction_count_side1;
    uint16_t triplet_offsets[3];
    vf2_player_pose_diag_field fields[VF2_PLAYER_POSE_DIAG_MAX_FIELDS];
    uint8_t field_count;
    uint8_t prefix;
    uint8_t prefix2;
    uint8_t side_angle;
    uint8_t player_triplet;
    uint8_t global_triplets;
} vf2_player_pose_diag_descriptor;

static const vf2_player_pose_diag_descriptor player_pose_diag_descriptors[] = {
    {
        1u, UINT32_C(0x0001668c), UINT32_C(0x00016710),
        UINT64_C(17), UINT64_C(16), {0u, 0u, 0u},
        {{UINT16_C(0x0be6), UINT32_C(0x36006c6c)}},
        1u, 1u, 0u, 1u, 0u, 0u
    },
    {
        2u, UINT32_C(0x00016748), UINT32_C(0x00016874),
        UINT64_C(22), UINT64_C(22),
        {UINT16_C(0x0bb2), UINT16_C(0x0bb0), UINT16_C(0x0bae)},
        {
            {UINT16_C(0x0c0e), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0c0c), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0c10), UINT32_C(0x37006e6e)},
            {UINT16_C(0x0150), UINT32_C(0x37006e6e)},
            {UINT16_C(0x014e), UINT32_C(0x36806d6d)}
        },
        5u, 1u, 0u, 0u, 1u, 0u
    },
    {
        5u, UINT32_C(0x000169dc), UINT32_C(0x00016b10),
        UINT64_C(16), UINT64_C(16), {0u, 0u, 0u},
        {
            {UINT16_C(0x015e), UINT32_C(0x37006e6e)},
            {UINT16_C(0x015c), UINT32_C(0x36806d6d)},
            {UINT16_C(0x015a), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bea), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0be8), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bec), UINT32_C(0x37006e6e)}
        },
        6u, 0u, 0u, 0u, 0u, 0u
    },
    {
        8u, UINT32_C(0x00016c6c), UINT32_C(0x00016da0),
        UINT64_C(16), UINT64_C(16), {0u, 0u, 0u},
        {
            {UINT16_C(0x016c), UINT32_C(0x37006e6e)},
            {UINT16_C(0x016a), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0168), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bf0), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0bee), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bf2), UINT32_C(0x37006e6e)}
        },
        6u, 0u, 0u, 0u, 0u, 0u
    },
    {
        9u, UINT32_C(0x00016dd8), UINT32_C(0x00016f04),
        UINT64_C(22), UINT64_C(22),
        {UINT16_C(0x0bc4), UINT16_C(0x0bc2), UINT16_C(0x0bc0)},
        {
            {UINT16_C(0x0c20), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0c1e), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0c22), UINT32_C(0x37006e6e)},
            {UINT16_C(0x0172), UINT32_C(0x37006e6e)},
            {UINT16_C(0x0170), UINT32_C(0x36806d6d)}
        },
        5u, 0u, 1u, 0u, 1u, 0u
    },
    {
        12u, UINT32_C(0x00017198), UINT32_C(0x000172fc),
        UINT64_C(26), UINT64_C(26), {0u, 0u, 0u},
        {
            {UINT16_C(0x0180), UINT32_C(0x37006e6e)},
            {UINT16_C(0x017e), UINT32_C(0x36806d6d)},
            {UINT16_C(0x017c), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bf6), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0bf4), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bf8), UINT32_C(0x37006e6e)}
        },
        6u, 0u, 0u, 0u, 0u, 1u
    },
    {
        15u, UINT32_C(0x0001745c), UINT32_C(0x000175bc),
        UINT64_C(25), UINT64_C(25), {0u, 0u, 0u},
        {
            {UINT16_C(0x018e), UINT32_C(0x37006e6e)},
            {UINT16_C(0x018c), UINT32_C(0x36806d6d)},
            {UINT16_C(0x018a), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bfc), UINT32_C(0x36806d6d)},
            {UINT16_C(0x0bfa), UINT32_C(0x36006c6c)},
            {UINT16_C(0x0bfe), UINT32_C(0x37006e6e)}
        },
        6u, 0u, 0u, 0u, 0u, 1u
    }
};

typedef struct vf2_player_pose_diag_plan {
    const vf2_player_pose_diag_descriptor *descriptor;
    uint32_t player;
    uint32_t port;
    uint32_t vector[3];
    uint32_t triplet[3];
    uint32_t global_triplets[9];
    uint32_t diagnostic_values[VF2_PLAYER_POSE_DIAG_MAX_FIELDS];
    uint32_t diagnostic_cursor;
    uint32_t side_angle;
    uint32_t current_angle;
    uint64_t instruction_count;
    uint8_t side;
} vf2_player_pose_diag_plan;

static const vf2_player_pose_diag_descriptor *player_pose_diag_descriptor_for(
    uint32_t selector,
    uint32_t entry
)
{
    size_t index = 0u;

    for (index = 0u;
         index < sizeof(player_pose_diag_descriptors) /
                     sizeof(player_pose_diag_descriptors[0]);
         ++index) {
        const vf2_player_pose_diag_descriptor *descriptor =
            &player_pose_diag_descriptors[index];
        if (descriptor->selector == selector && descriptor->entry == entry) {
            return descriptor;
        }
    }
    return NULL;
}

static vf2_status player_pose_diag_read_i16(
    const vf2_model2a *machine,
    uint32_t address,
    uint32_t *value
)
{
    uint16_t raw = 0u;
    vf2_status status = player_tail_read_u16(machine, address, &raw);

    if (status == VF2_OK && value != NULL) {
        *value = (uint32_t)(int32_t)(int16_t)raw;
    }
    return value == NULL ? VF2_ERROR_INVALID_ARGUMENT : status;
}

static vf2_status player_pose_prepare_diagnostics(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    vf2_player_pose_diag_plan *plan
)
{
    const vf2_player_pose_diag_descriptor *descriptor = NULL;
    uint32_t cursor = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    descriptor = player_pose_diag_descriptor_for(selector, cpu->ip);
    if (descriptor == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(plan, 0, sizeof(*plan));
    plan->descriptor = descriptor;
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    plan->port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                 cpu->registers[VF2_I960_G0_REGISTER + 12u];
    if (plan->player == 0u || plan->port == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; index < 3u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            plan->player + UINT32_C(0x80) + selector * UINT32_C(12) +
                (uint32_t)index * UINT32_C(4),
            &plan->vector[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = player_tail_read_u8(
        machine, plan->player + UINT32_C(4), &plan->side
    );
    if (status != VF2_OK) {
        return status;
    }

    if (descriptor->side_angle != 0u) {
        status = player_pose_diag_read_i16(
            machine, plan->player + UINT32_C(0x26), &plan->current_angle
        );
        if (status != VF2_OK) {
            return status;
        }
        plan->side_angle =
            ((plan->side & UINT8_C(1)) != 0u
                ? UINT32_C(0x3b00) : UINT32_C(0x3a00)) +
            selector * UINT32_C(12);
    }

    if (descriptor->player_triplet != 0u) {
        for (index = 0u; index < 3u; ++index) {
            status = player_pose_diag_read_i16(
                machine,
                plan->player + (uint32_t)descriptor->triplet_offsets[index],
                &plan->triplet[index]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    if (descriptor->global_triplets != 0u) {
        for (index = 0u; index < 9u; ++index) {
            status = vf2_model2a_read_u32(
                machine,
                VF2_PLAYER_POSE_DIAG_GLOBAL_TRIPLES +
                    (uint32_t)index * UINT32_C(4),
                &plan->global_triplets[index]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }

    status = vf2_model2a_read_u32(
        machine, VF2_PLAYER_POSE_DIAG_CURSOR, &cursor
    );
    if (status != VF2_OK) {
        return status;
    }
    plan->diagnostic_cursor = cursor;

    for (index = 0u; index < descriptor->field_count; ++index) {
        uint32_t simulated_cursor = plan->diagnostic_cursor;

        status = player_pose_diag_read_i16(
            machine,
            plan->player + (uint32_t)descriptor->fields[index].offset,
            &plan->diagnostic_values[index]
        );
        if (status != VF2_OK) {
            return status;
        }
        if (plan->diagnostic_values[index] == 0u) {
            continue;
        }
        /* Preflight the exact buffer write. The ROM advances the 32-bit cursor
         * but commits only its low byte with STOB, so carry out of bit 7 is
         * deliberately discarded for the next diagnostic. */
        {
            uint32_t ignored = 0u;
            status = vf2_model2a_read_u32(
                machine,
                VF2_PLAYER_POSE_DIAG_BASE + simulated_cursor,
                &ignored
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        simulated_cursor += UINT32_C(4);
        plan->diagnostic_cursor =
            (plan->diagnostic_cursor & ~UINT32_C(0xff)) |
            (simulated_cursor & UINT32_C(0xff));
    }

    plan->instruction_count =
        (plan->side & UINT8_C(1)) != 0u
            ? descriptor->base_instruction_count_side1
            : descriptor->base_instruction_count_side0;
    for (index = 0u; index < descriptor->field_count; ++index) {
        if (plan->diagnostic_values[index] != 0u) {
            plan->instruction_count += UINT64_C(6);
        }
    }
    return VF2_OK;
}

static vf2_status player_pose_diag_emit(
    vf2_model2a *machine,
    uint32_t port,
    uint32_t player,
    uint32_t *cursor,
    uint32_t value,
    uint32_t command
)
{
    uint8_t low = 0u;
    uint32_t next = 0u;
    vf2_status status = VF2_OK;

    if (value == 0u || cursor == NULL) {
        return cursor == NULL ? VF2_ERROR_INVALID_ARGUMENT : VF2_OK;
    }
    status = vf2_model2a_write_u32(
        machine, VF2_PLAYER_POSE_DIAG_BASE + *cursor, value
    );
    next = *cursor + UINT32_C(4);
    low = (uint8_t)next;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_PLAYER_POSE_DIAG_CURSOR, &low, sizeof(low)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, command);
    }
    if (status == VF2_OK) {
        *cursor = (*cursor & ~UINT32_C(0xff)) | (next & UINT32_C(0xff));
    }
    (void)player;
    return status;
}

static vf2_status player_pose_apply_diagnostics(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_pose_diag_plan *plan
)
{
    const vf2_player_pose_diag_descriptor *descriptor =
        plan != NULL ? plan->descriptor : NULL;
    uint32_t cursor = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL || descriptor == NULL ||
        cpu->ip != descriptor->entry ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (descriptor->prefix2 != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_DIAG_PREFIX2
        );
    }
    if (status == VF2_OK && descriptor->prefix != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_DIAG_PREFIX
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_DIAG_VECTOR_COMMAND
        );
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(
            machine,
            plan->port + (uint32_t)index * UINT32_C(4),
            plan->vector[index]
        );
    }
    if (status == VF2_OK && descriptor->side_angle != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_DIAG_ANGLE_COMMAND
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, plan->port, plan->side_angle
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, plan->port, plan->current_angle
            );
        }
    }
    if (status == VF2_OK && descriptor->player_triplet != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_DIAG_TRIPLET_COMMAND
        );
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_write_u32(
                machine,
                plan->port + (uint32_t)index * UINT32_C(4),
                plan->triplet[index]
            );
        }
    }
    if (status == VF2_OK && descriptor->global_triplets != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_DIAG_GLOBAL_COMMAND
        );
        for (index = 0u; status == VF2_OK && index < 9u; index += 3u) {
            size_t component = 0u;
            for (component = 0u; status == VF2_OK && component < 3u;
                 ++component) {
                status = vf2_model2a_write_u32(
                    machine,
                    plan->port + (uint32_t)component * UINT32_C(4),
                    plan->global_triplets[index + component]
                );
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_PLAYER_POSE_DIAG_CURSOR, &cursor
    );
    if (status != VF2_OK) {
        return status;
    }
    for (index = 0u; index < descriptor->field_count; ++index) {
        status = player_pose_diag_emit(
            machine,
            plan->port,
            plan->player,
            &cursor,
            plan->diagnostic_values[index],
            descriptor->fields[index].command
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->executed_instructions += plan->instruction_count;
    cpu->ip = descriptor->stop;
    return VF2_OK;
}

static vf2_status player_pose_execute_diagnostics(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector
)
{
    vf2_player_pose_diag_plan plan;
    vf2_status status = player_pose_prepare_diagnostics(
        machine, cpu, selector, &plan
    );

    if (status != VF2_OK) {
        return status;
    }
    return player_pose_apply_diagnostics(machine, cpu, &plan);
}
