#define VF2_PLAYER_176A0_ENTRY UINT32_C(0x000176a0)
#define VF2_PLAYER_176A0_DATA_BASE UINT32_C(0x020078a8)
#define VF2_PLAYER_176A0_LIST_TABLE UINT32_C(0x02007bee)
#define VF2_PLAYER_176A0_COUNT_TABLE UINT32_C(0x02007c68)
#define VF2_PLAYER_176A0_STATE_POINTER UINT32_C(0x00500828)
#define VF2_PLAYER_176A0_COMMAND_A UINT32_C(0x1c803939)
#define VF2_PLAYER_176A0_COMMAND_B UINT32_C(0x14802929)

typedef struct vf2_player_176a0_plan {
    uint32_t player;
    uint32_t port;
    uint32_t list_pointer;
    uint32_t state_word;
    uint32_t return_address;
    uint32_t selector;
    uint8_t count;
    uint64_t instruction_count;
} vf2_player_176a0_plan;

static vf2_status player_prepare_176a0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_player_176a0_plan *plan
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t state_pointer = 0u;
    uint32_t selector = 0u;
    uint8_t count = 0u;
    uint8_t table_index = 0u;
    size_t iteration = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_176A0_ENTRY ||
        cpu->local_frame_depth == 0u || player == 0u || port == 0u ||
        (cpu->registers[0] & UINT32_C(7)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    selector = cpu->registers[VF2_I960_G0_REGISTER];
    plan->player = player;
    plan->port = port;
    plan->selector = selector;
    plan->return_address =
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2];
    if (plan->return_address == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine,
        VF2_PLAYER_176A0_LIST_TABLE + selector * UINT32_C(4),
        &plan->list_pointer
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_PLAYER_176A0_COUNT_TABLE + selector,
            &count,
            sizeof(count)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_176A0_STATE_POINTER, &state_pointer
        );
    }
    if (status == VF2_OK && state_pointer != 0u) {
        status = vf2_model2a_read_u32(
            machine, state_pointer, &plan->state_word
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (state_pointer == 0u || plan->list_pointer == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    plan->count = count;

    /* Preflight every table/list/destination access before the first copro
     * write. MAIN_DATA is immutable, so the execution pass can safely reread
     * the same records after validation. */
    for (iteration = 0u; iteration < (size_t)count; ++iteration) {
        const uint32_t descending =
            (uint32_t)count - UINT32_C(1) - (uint32_t)iteration;
        uint32_t word = 0u;
        size_t component = 0u;

        status = vf2_model2a_read(
            machine,
            plan->list_pointer + descending,
            &table_index,
            sizeof(table_index)
        );
        if (status != VF2_OK) {
            return status;
        }
        for (component = 0u; component < 4u; ++component) {
            status = vf2_model2a_read_u32(
                machine,
                VF2_PLAYER_176A0_DATA_BASE +
                    (uint32_t)table_index * UINT32_C(4) +
                    (uint32_t)component * UINT32_C(4),
                &word
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        if ((plan->state_word & (UINT32_C(1) << 31u)) == 0u) {
            for (component = 0u; component < 4u; ++component) {
                status = vf2_model2a_read_u32(
                    machine,
                    player + UINT32_C(0x0d00) +
                        (uint32_t)table_index * UINT32_C(4) +
                        (uint32_t)component * UINT32_C(4),
                    &word
                );
                if (status != VF2_OK) {
                    return status;
                }
            }
        }
    }

    /* Five entry instructions, final cmpdeci/bge pair and RET. Each active
     * item is 9 instructions when bit 31 suppresses command B, otherwise 14. */
    plan->instruction_count = UINT64_C(8) +
        (uint64_t)count *
            (((plan->state_word & (UINT32_C(1) << 31u)) != 0u)
                ? UINT64_C(9) : UINT64_C(14));
    return VF2_OK;
}

static vf2_status player_apply_176a0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_176a0_plan *plan
)
{
    size_t iteration = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != VF2_PLAYER_176A0_ENTRY ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u] ||
        cpu->local_frame_depth == 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            plan->return_address) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (iteration = 0u; iteration < (size_t)plan->count; ++iteration) {
        const uint32_t descending =
            (uint32_t)plan->count - UINT32_C(1) - (uint32_t)iteration;
        uint8_t table_index = 0u;
        uint32_t words[4] = {0u, 0u, 0u, 0u};
        size_t component = 0u;

        status = vf2_model2a_read(
            machine,
            plan->list_pointer + descending,
            &table_index,
            sizeof(table_index)
        );
        for (component = 0u; status == VF2_OK && component < 4u; ++component) {
            status = vf2_model2a_read_u32(
                machine,
                VF2_PLAYER_176A0_DATA_BASE +
                    (uint32_t)table_index * UINT32_C(4) +
                    (uint32_t)component * UINT32_C(4),
                &words[component]
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, plan->port, VF2_PLAYER_176A0_COMMAND_A
            );
        }
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = vf2_model2a_write_u32(
                machine,
                plan->port + (uint32_t)component * UINT32_C(4),
                words[component]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if ((plan->state_word & (UINT32_C(1) << 31u)) != 0u) {
            continue;
        }

        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_176A0_COMMAND_B
        );
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = vf2_model2a_write_u32(
                machine,
                plan->port + (uint32_t)component * UINT32_C(4),
                words[component]
            );
        }
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine,
                plan->port + (uint32_t)component * UINT32_C(4),
                &words[component]
            );
        }
        for (component = 0u; status == VF2_OK && component < 4u; ++component) {
            status = vf2_model2a_write_u32(
                machine,
                plan->player + UINT32_C(0x0d00) +
                    (uint32_t)table_index * UINT32_C(4) +
                    (uint32_t)component * UINT32_C(4),
                words[component]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    /* The terminating cmpdeci compares 0 with the final zero count before
     * decrementing it, leaving the architectural condition code equal. */
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->ip = UINT32_C(0x0001770c);
    cpu->executed_instructions += plan->instruction_count - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == plan->return_address
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_execute_176a0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    vf2_player_176a0_plan plan;
    vf2_status status = player_prepare_176a0(machine, cpu, &plan);

    if (status != VF2_OK) {
        return status;
    }
    if (max_steps != 0u && max_steps < plan.instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return player_apply_176a0(machine, cpu, &plan);
}
