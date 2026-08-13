#define VF2_PLAYER_POSE_TEN_ENTRY UINT32_C(0x00016f3c)
#define VF2_PLAYER_POSE_TEN_STOP UINT32_C(0x000170d4)
#define VF2_PLAYER_POSE_TEN_PREFIX UINT32_C(0x00800101)
#define VF2_PLAYER_POSE_TEN_STREAM_SETUP UINT32_C(0x06000c0c)
#define VF2_PLAYER_POSE_TEN_STREAM_REPEAT UINT32_C(0x09801313)
#define VF2_PLAYER_POSE_TEN_SIDE_SETUP UINT32_C(0x1b803737)
#define VF2_PLAYER_POSE_TEN_TRANSFORM UINT32_C(0x14802929)
#define VF2_PLAYER_POSE_TEN_POST UINT32_C(0x01000202)
#define VF2_PLAYER_POSE_TEN_VECTOR UINT32_C(0x03000606)
#define VF2_PLAYER_POSE_TEN_ANGLE UINT32_C(0x34806969)
#define VF2_PLAYER_POSE_TEN_MAX_RECORDS UINT32_C(1024)

typedef struct vf2_player_pose_ten_plan {
    uint32_t player;
    uint32_t port;
    uint32_t geometry_base;
    uint32_t geometry_port;
    uint32_t object;
    uint32_t record_count;
    uint32_t record_iterations;
    uint32_t index_pointer;
    uint32_t vector[3];
    uint32_t side;
    uint32_t side_angle;
    uint32_t current_angle;
    uint64_t instruction_count;
    uint8_t object_present;
    vf2_i960_compare_result final_compare;
} vf2_player_pose_ten_plan;

static vf2_status player_pose_ten_write_triplet(
    vf2_model2a *machine,
    uint32_t address,
    const uint32_t values[3]
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < 3u; ++index) {
        status = vf2_model2a_write_u32(
            machine, address + (uint32_t)index * UINT32_C(4), values[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static vf2_status player_pose_ten_read_triplet(
    const vf2_model2a *machine,
    uint32_t address,
    uint32_t values[3]
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < 3u; ++index) {
        status = vf2_model2a_read_u32(
            machine, address + (uint32_t)index * UINT32_C(4), &values[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static vf2_status player_pose_ten_preflight_record(
    const vf2_model2a *machine,
    const vf2_player_pose_ten_plan *plan,
    uint32_t iteration,
    uint64_t *extra_instructions
)
{
    const uint64_t record64 =
        (uint64_t)plan->object + UINT64_C(0xd4) +
        (uint64_t)iteration * UINT64_C(0x28);
    const uint64_t index64 =
        (uint64_t)plan->index_pointer + (uint64_t)iteration * UINT64_C(2);
    uint32_t ignored = 0u;
    uint8_t indices[2] = {0u, 0u};
    size_t word = 0u;
    vf2_status status = VF2_OK;

    if (record64 > UINT32_MAX || index64 + UINT64_C(1) > UINT32_MAX ||
        extra_instructions == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (word = 0u; word < 10u; ++word) {
        status = vf2_model2a_read_u32(
            machine,
            (uint32_t)record64 + (uint32_t)word * UINT32_C(4),
            &ignored
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_read(
        machine, (uint32_t)index64, indices, sizeof(indices)
    );
    if (status != VF2_OK) {
        return status;
    }
    for (word = 0u; word < 2u; ++word) {
        if (indices[word] >= UINT8_C(8)) {
            uint32_t triple[3] = {0u, 0u, 0u};
            status = player_pose_ten_read_triplet(
                machine,
                plan->player + UINT32_C(0x1800) +
                    (uint32_t)indices[word] * UINT32_C(12),
                triple
            );
            if (status != VF2_OK) {
                return status;
            }
            *extra_instructions += UINT64_C(2);
        }
    }
    return VF2_OK;
}

static vf2_status player_pose_prepare_ten(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_pose_ten_plan *plan
)
{
    uint32_t state_pointer = 0u;
    uint32_t ignored = 0u;
    uint16_t angle = 0u;
    uint8_t side = 0u;
    uint64_t index_pointer64 = 0u;
    uint64_t extra_instructions = 0u;
    uint32_t iteration = 0u;
    size_t component = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_POSE_TEN_ENTRY) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    plan->port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                 cpu->registers[VF2_I960_G0_REGISTER + 12u];
    plan->geometry_base = cpu->registers[VF2_I960_G0_REGISTER + 10u];
    plan->geometry_port =
        plan->geometry_base + cpu->registers[VF2_I960_G0_REGISTER + 12u];
    if (plan->player == 0u || plan->port == 0u ||
        plan->geometry_base == 0u || plan->geometry_port == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine, plan->player + UINT32_C(0x190), &state_pointer
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, state_pointer + UINT32_C(0x38), &plan->object
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u8(
            machine, plan->player + UINT32_C(4), &side
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, plan->player + UINT32_C(0x26), &angle
        );
    }
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_read_u32(
            machine,
            plan->player + UINT32_C(0xf8) +
                (uint32_t)component * UINT32_C(4),
            &plan->vector[component]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    plan->side = (uint32_t)side;
    plan->current_angle = (uint32_t)(int32_t)(int16_t)angle;
    plan->side_angle =
        ((side & UINT8_C(1)) != 0u ? UINT32_C(0x3b78) : UINT32_C(0x3a78));

    if (plan->object == 0u) {
        plan->instruction_count = UINT64_C(3) +
            (((side & UINT8_C(1)) != 0u) ? UINT64_C(19) : UINT64_C(20));
        return VF2_OK;
    }
    plan->object_present = 1u;

    /* Sixteen transform sources and destinations are fixed-size. */
    for (iteration = 0u; iteration < 16u; ++iteration) {
        uint32_t triple[3] = {0u, 0u, 0u};
        status = player_pose_ten_read_triplet(
            machine,
            plan->object + UINT32_C(0x70) + iteration * UINT32_C(12),
            triple
        );
        if (status != VF2_OK) {
            return status;
        }
        status = player_pose_ten_read_triplet(
            machine,
            plan->player + UINT32_C(0x1860) + iteration * UINT32_C(12),
            triple
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    status = vf2_model2a_read_u32(
        machine, plan->object + UINT32_C(0xd0), &plan->record_count
    );
    if (status != VF2_OK || plan->record_count > VF2_PLAYER_POSE_TEN_MAX_RECORDS) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    plan->record_iterations =
        plan->record_count == 0u ? UINT32_C(1) : plan->record_count;
    index_pointer64 =
        (uint64_t)plan->object + UINT64_C(0xd4) +
        ((uint64_t)plan->record_count * UINT64_C(10) + UINT64_C(1)) *
            UINT64_C(4);
    if (index_pointer64 > UINT32_MAX) {
        return VF2_ERROR_UNSUPPORTED;
    }
    plan->index_pointer = (uint32_t)index_pointer64;

    for (iteration = 0u; iteration < plan->record_iterations; ++iteration) {
        status = player_pose_ten_preflight_record(
            machine, plan, iteration, &extra_instructions
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    {
        const uint64_t final_record =
            (uint64_t)plan->object + UINT64_C(0xd4) +
            (uint64_t)plan->record_iterations * UINT64_C(0x28);
        if (final_record > UINT32_MAX) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine, (uint32_t)final_record, &ignored
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_read_u32(
        machine, plan->geometry_base + UINT32_C(0x50), &ignored
    );
    if (status != VF2_OK) {
        return status;
    }

    plan->final_compare = plan->record_count == 0u
        ? VF2_I960_COMPARE_GREATER : VF2_I960_COMPARE_EQUAL;
    plan->instruction_count = UINT64_C(198) +
        (uint64_t)plan->record_iterations * UINT64_C(16) +
        extra_instructions +
        (((side & UINT8_C(1)) != 0u) ? UINT64_C(19) : UINT64_C(20));
    return VF2_OK;
}

static vf2_status player_pose_apply_ten(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_pose_ten_plan *plan
)
{
    uint32_t ignored = 0u;
    uint32_t iteration = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_POSE_TEN_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (plan->object_present != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_TEN_PREFIX
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, plan->port, VF2_PLAYER_POSE_TEN_STREAM_SETUP
            );
        }
        for (iteration = 0u; status == VF2_OK && iteration < 3u; ++iteration) {
            status = vf2_model2a_write_u32(
                machine, plan->port, VF2_PLAYER_POSE_TEN_STREAM_REPEAT
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, plan->port, &ignored);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, plan->port, VF2_PLAYER_POSE_TEN_SIDE_SETUP
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, plan->port, plan->side);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, plan->port, UINT32_C(12));
        }
        if (status != VF2_OK) {
            return status;
        }

        for (iteration = 0u; iteration < 16u; ++iteration) {
            uint32_t triple[3] = {0u, 0u, 0u};

            status = player_pose_ten_read_triplet(
                machine,
                plan->object + UINT32_C(0x70) + iteration * UINT32_C(12),
                triple
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, plan->port, VF2_PLAYER_POSE_TEN_TRANSFORM
                );
            }
            if (status == VF2_OK) {
                status = player_pose_ten_write_triplet(
                    machine, plan->port, triple
                );
            }
            if (status == VF2_OK) {
                status = player_pose_ten_read_triplet(
                    machine, plan->port, triple
                );
            }
            if (status == VF2_OK) {
                status = player_pose_ten_write_triplet(
                    machine,
                    plan->player + UINT32_C(0x1860) +
                        iteration * UINT32_C(12),
                    triple
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }

        {
            uint32_t record = plan->object + UINT32_C(0xd4);
            uint32_t indices = plan->index_pointer;
            const uint32_t palette = plan->player + UINT32_C(0x1800);
            const uint32_t side_word = plan->side << 8u;
            const uint32_t count_word = plan->record_count * UINT32_C(10) +
                                        UINT32_C(1);

            status = vf2_model2a_write_u32(
                machine, plan->geometry_base + UINT32_C(0x50), side_word
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, plan->geometry_port, side_word
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, plan->geometry_port, count_word
                );
            }
            if (status != VF2_OK) {
                return status;
            }

            for (iteration = 0u; iteration < plan->record_iterations;
                 ++iteration) {
                uint32_t header[4] = {0u, 0u, 0u, 0u};
                uint8_t index_pair[2] = {0u, 0u};
                size_t word = 0u;

                for (word = 0u; word < 4u; ++word) {
                    status = vf2_model2a_read_u32(
                        machine,
                        record + (uint32_t)word * UINT32_C(4),
                        &header[word]
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                }
                for (word = 0u; word < 4u; ++word) {
                    status = vf2_model2a_write_u32(
                        machine,
                        plan->geometry_port + (uint32_t)word * UINT32_C(4),
                        header[word]
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                }
                status = vf2_model2a_read(
                    machine, indices, index_pair, sizeof(index_pair)
                );
                if (status != VF2_OK) {
                    return status;
                }
                for (word = 0u; word < 2u; ++word) {
                    uint32_t triple[3] = {0u, 0u, 0u};
                    const uint32_t source = index_pair[word] < UINT8_C(8)
                        ? record + UINT32_C(0x10) +
                            (uint32_t)word * UINT32_C(12)
                        : palette + (uint32_t)index_pair[word] * UINT32_C(12);

                    status = player_pose_ten_read_triplet(
                        machine, source, triple
                    );
                    if (status == VF2_OK) {
                        status = player_pose_ten_write_triplet(
                            machine, plan->geometry_port, triple
                        );
                    }
                    if (status != VF2_OK) {
                        return status;
                    }
                }
                record += UINT32_C(0x28);
                indices += UINT32_C(2);
            }
            status = vf2_model2a_read_u32(machine, record, &ignored);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, plan->geometry_port, ignored
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, plan->port, VF2_PLAYER_POSE_TEN_POST
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }

        cpu->registers[10] = plan->player + UINT32_C(0x1800);
        cpu->compare_result = plan->final_compare;
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->arithmetic_control |= plan->final_compare == VF2_I960_COMPARE_EQUAL
            ? UINT32_C(2) : UINT32_C(1);
    }

    status = vf2_model2a_write_u32(
        machine, plan->port, VF2_PLAYER_POSE_TEN_PREFIX
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_TEN_VECTOR
        );
    }
    if (status == VF2_OK) {
        status = player_pose_ten_write_triplet(
            machine, plan->port, plan->vector
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_TEN_ANGLE
        );
    }
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
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += plan->instruction_count;
    cpu->ip = VF2_PLAYER_POSE_TEN_STOP;
    return VF2_OK;
}

static vf2_status player_pose_execute_ten(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_player_pose_ten_plan plan;
    vf2_status status = player_pose_prepare_ten(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    return player_pose_apply_ten(machine, cpu, &plan);
}
