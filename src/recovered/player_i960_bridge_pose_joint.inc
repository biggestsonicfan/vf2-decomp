#define VF2_PLAYER_POSE_JOINT_COUNT 16u
#define VF2_PLAYER_POSE_JOINT_COMMAND UINT32_C(0x1a803535)
#define VF2_PLAYER_POSE_JOINT_READ_COMMAND UINT32_C(0x07800f0f)

static const uint32_t player_pose_joint_entries[VF2_PLAYER_POSE_JOINT_COUNT] = {
    UINT32_C(0x00016654), UINT32_C(0x00016710),
    UINT32_C(0x00016874), UINT32_C(0x00016918),
    UINT32_C(0x000169a4), UINT32_C(0x00016b10),
    UINT32_C(0x00016ba8), UINT32_C(0x00016c34),
    UINT32_C(0x00016da0), UINT32_C(0x00016f04),
    UINT32_C(0x000170d4), UINT32_C(0x00017160),
    UINT32_C(0x000172fc), UINT32_C(0x00017398),
    UINT32_C(0x00017424), UINT32_C(0x000175bc)
};

static const uint32_t player_pose_joint_returns[VF2_PLAYER_POSE_JOINT_COUNT] = {
    UINT32_C(0x0001668c), UINT32_C(0x00016748),
    UINT32_C(0x000168ac), UINT32_C(0x00016950),
    UINT32_C(0x000169dc), UINT32_C(0x00016b48),
    UINT32_C(0x00016be0), UINT32_C(0x00016c6c),
    UINT32_C(0x00016dd8), UINT32_C(0x00016f3c),
    UINT32_C(0x0001710c), UINT32_C(0x00017198),
    UINT32_C(0x00017334), UINT32_C(0x000173d0),
    UINT32_C(0x0001745c), UINT32_C(0x000175f4)
};

typedef struct vf2_player_pose_joint_plan {
    uint32_t player;
    uint32_t port;
    uint32_t entry;
    uint32_t return_address;
    uint32_t selector;
    uint8_t side;
    uint64_t instruction_count;
    vf2_player_176a0_plan helper;
} vf2_player_pose_joint_plan;

static vf2_status player_pose_prepare_joint(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    vf2_player_pose_joint_plan *plan
)
{
    vf2_i960_cpu helper_cpu;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        selector >= VF2_PLAYER_POSE_JOINT_COUNT ||
        cpu->ip != player_pose_joint_entries[selector] ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    plan->port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                 cpu->registers[VF2_I960_G0_REGISTER + 12u];
    plan->entry = cpu->ip;
    plan->return_address = player_pose_joint_returns[selector];
    plan->selector = selector;
    if (plan->player == 0u || plan->port == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = player_tail_read_u8(
        machine, plan->player + UINT32_C(4), &plan->side
    );
    if (status != VF2_OK) {
        return status;
    }
    /* The joint result write is a contiguous three-word store. Preflight the
     * destination before issuing any copro command. */
    {
        uint32_t ignored = 0u;
        size_t component = 0u;
        for (component = 0u; component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine,
                plan->player + UINT32_C(0x01f4) +
                    selector * UINT32_C(12) +
                    (uint32_t)component * UINT32_C(4),
                &ignored
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }

    /* Prepare the real 0x176a0 callee against a CPU copy. Entering a procedure
     * changes only architectural CPU state, so this validates the complete
     * helper before the joint tail performs its first hardware write. */
    helper_cpu = *cpu;
    helper_cpu.registers[VF2_I960_G0_REGISTER] = selector;
    status = vf2_i960_cpu_enter_procedure(
        &helper_cpu, VF2_PLAYER_176A0_ENTRY, plan->return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    status = player_prepare_176a0(machine, &helper_cpu, &plan->helper);
    if (status != VF2_OK) {
        return status;
    }

    /* Eleven instructions prepare/store/read the joint tuple, then the CALL
     * itself executes before the helper's own instruction count. */
    plan->instruction_count = UINT64_C(12) + plan->helper.instruction_count;
    return VF2_OK;
}

static vf2_status player_pose_apply_joint(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_pose_joint_plan *plan
)
{
    uint32_t tuple[3] = {0u, 0u, 0u};
    size_t component = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL ||
        cpu->ip != plan->entry ||
        plan->selector >= VF2_PLAYER_POSE_JOINT_COUNT ||
        plan->entry != player_pose_joint_entries[plan->selector] ||
        plan->return_address != player_pose_joint_returns[plan->selector] ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(
        machine, plan->port, VF2_PLAYER_POSE_JOINT_COMMAND
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, plan->port, plan->side);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, plan->selector * UINT32_C(12)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_JOINT_READ_COMMAND
        );
    }
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_read_u32(
            machine,
            plan->port + (uint32_t)component * UINT32_C(4),
            &tuple[component]
        );
    }
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_write_u32(
            machine,
            plan->player + UINT32_C(0x01f4) +
                plan->selector * UINT32_C(12) +
                (uint32_t)component * UINT32_C(4),
            tuple[component]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* Reconstruct the live caller register state at the architectural CALL. */
    cpu->registers[12] = tuple[0];
    cpu->registers[13] = tuple[1];
    cpu->registers[14] = tuple[2];
    cpu->registers[15] = VF2_PLAYER_POSE_JOINT_READ_COMMAND;
    cpu->registers[VF2_I960_G0_REGISTER] = plan->selector;
    cpu->executed_instructions += UINT64_C(11);

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_PLAYER_176A0_ENTRY, plan->return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;

    status = player_apply_176a0(machine, cpu, &plan->helper);
    if (status != VF2_OK) {
        return status;
    }
    return cpu->ip == plan->return_address
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_pose_execute_joint(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    uint64_t max_steps
)
{
    vf2_player_pose_joint_plan plan;
    vf2_status status = player_pose_prepare_joint(
        machine, cpu, selector, &plan
    );

    if (status != VF2_OK) {
        return status;
    }
    if (max_steps != 0u && max_steps < plan.instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return player_pose_apply_joint(machine, cpu, &plan);
}

static int player_pose_joint_selector(
    uint32_t entry,
    uint32_t stop_address,
    uint32_t *selector
)
{
    uint32_t index = 0u;

    if (selector == NULL) {
        return 0;
    }
    for (index = 0u; index < VF2_PLAYER_POSE_JOINT_COUNT; ++index) {
        if (entry == player_pose_joint_entries[index] &&
            stop_address == player_pose_joint_returns[index]) {
            *selector = index;
            return 1;
        }
    }
    return 0;
}
