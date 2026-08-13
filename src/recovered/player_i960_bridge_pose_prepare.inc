#define VF2_PLAYER_POSE_PREP_VECTOR_COMMAND UINT32_C(0x03000606)
#define VF2_PLAYER_POSE_PREP_ANGLE_COMMAND UINT32_C(0x34806969)
#define VF2_PLAYER_POSE_PREP_PREFIX_A UINT32_C(0x01000202)
#define VF2_PLAYER_POSE_PREP_PREFIX_B UINT32_C(0x00800101)

typedef struct vf2_player_pose_prepare_descriptor {
    uint32_t selector;
    uint32_t entry;
    uint32_t stop;
    uint8_t prefix_a;
    uint8_t prefix_b;
    uint8_t prefix_instruction_count;
} vf2_player_pose_prepare_descriptor;

static const vf2_player_pose_prepare_descriptor player_pose_prepare_descriptors[] = {
    {3u,  UINT32_C(0x000168ac), UINT32_C(0x00016918), 1u, 1u, 5u},
    {4u,  UINT32_C(0x00016950), UINT32_C(0x000169a4), 0u, 0u, 1u},
    {6u,  UINT32_C(0x00016b48), UINT32_C(0x00016ba8), 1u, 0u, 3u},
    {7u,  UINT32_C(0x00016be0), UINT32_C(0x00016c34), 0u, 0u, 1u},
    {11u, UINT32_C(0x0001710c), UINT32_C(0x00017160), 0u, 0u, 1u},
    {13u, UINT32_C(0x00017334), UINT32_C(0x00017398), 1u, 0u, 4u},
    {14u, UINT32_C(0x000173d0), UINT32_C(0x00017424), 0u, 0u, 1u},
};

typedef struct vf2_player_pose_prepare_plan {
    const vf2_player_pose_prepare_descriptor *descriptor;
    uint32_t player;
    uint32_t port;
    uint32_t vector[3];
    uint32_t side_angle;
    uint32_t current_angle;
    uint64_t instruction_count;
} vf2_player_pose_prepare_plan;

static const vf2_player_pose_prepare_descriptor *
player_pose_prepare_descriptor_for(uint32_t selector, uint32_t entry)
{
    size_t index = 0u;

    for (index = 0u;
         index < sizeof(player_pose_prepare_descriptors) /
                     sizeof(player_pose_prepare_descriptors[0]);
         ++index) {
        const vf2_player_pose_prepare_descriptor *descriptor =
            &player_pose_prepare_descriptors[index];
        if (descriptor->selector == selector && descriptor->entry == entry) {
            return descriptor;
        }
    }
    return NULL;
}

static vf2_status player_pose_prepare_preparation(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    vf2_player_pose_prepare_plan *plan
)
{
    const vf2_player_pose_prepare_descriptor *descriptor = NULL;
    uint16_t angle = 0u;
    uint8_t side = 0u;
    size_t component = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    descriptor = player_pose_prepare_descriptor_for(selector, cpu->ip);
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

    for (component = 0u; component < 3u; ++component) {
        status = vf2_model2a_read_u32(
            machine,
            plan->player + UINT32_C(0x80) + selector * UINT32_C(12) +
                (uint32_t)component * UINT32_C(4),
            &plan->vector[component]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = player_tail_read_u8(
        machine, plan->player + UINT32_C(4), &side
    );
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, plan->player + UINT32_C(0x26), &angle
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    plan->side_angle =
        ((side & UINT8_C(1)) != 0u ? UINT32_C(0x3b00) : UINT32_C(0x3a00)) +
        selector * UINT32_C(12);
    plan->current_angle = (uint32_t)(int32_t)(int16_t)angle;
    /* Common path is 14 instructions for side=1 and 15 for side=0 because
     * the clear-side arm executes the explicit branch to the merge block. */
    plan->instruction_count =
        (uint64_t)descriptor->prefix_instruction_count +
        (((side & UINT8_C(1)) != 0u) ? UINT64_C(14) : UINT64_C(15));
    return VF2_OK;
}

static vf2_status player_pose_apply_preparation(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_player_pose_prepare_plan *plan
)
{
    const vf2_player_pose_prepare_descriptor *descriptor =
        plan != NULL ? plan->descriptor : NULL;
    size_t component = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || plan == NULL || descriptor == NULL ||
        cpu->ip != descriptor->entry ||
        plan->player != cpu->registers[VF2_I960_G0_REGISTER + 7u] ||
        plan->port != cpu->registers[VF2_I960_G0_REGISTER + 11u] +
            cpu->registers[VF2_I960_G0_REGISTER + 12u]) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (descriptor->prefix_a != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_PREP_PREFIX_A
        );
    }
    if (status == VF2_OK && descriptor->prefix_b != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_PREP_PREFIX_B
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_PREP_VECTOR_COMMAND
        );
    }
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_write_u32(
            machine,
            plan->port + (uint32_t)component * UINT32_C(4),
            plan->vector[component]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->port, VF2_PLAYER_POSE_PREP_ANGLE_COMMAND
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

    /* ldt leaves r12-r14 live and ldis leaves the signed angle in r15 at the
     * joint-tail boundary. No comparison instruction in this template changes
     * the architectural condition code. */
    cpu->registers[12] = plan->vector[0];
    cpu->registers[13] = plan->vector[1];
    cpu->registers[14] = plan->vector[2];
    cpu->registers[15] = plan->current_angle;
    cpu->executed_instructions += plan->instruction_count;
    cpu->ip = descriptor->stop;
    return VF2_OK;
}

static vf2_status player_pose_execute_preparation(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector
)
{
    vf2_player_pose_prepare_plan plan;
    vf2_status status = player_pose_prepare_preparation(
        machine, cpu, selector, &plan
    );

    if (status != VF2_OK) {
        return status;
    }
    return player_pose_apply_preparation(machine, cpu, &plan);
}
