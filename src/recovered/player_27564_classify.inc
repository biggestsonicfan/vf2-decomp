typedef enum vf2_player_27564_outcome {
    VF2_PLAYER_27564_SKIP = 0,
    VF2_PLAYER_27564_EARLY_RETURN,
    VF2_PLAYER_27564_APPLY
} vf2_player_27564_outcome;

typedef struct vf2_player_27564_plan {
    vf2_player_27564_outcome outcome;
    uint16_t left_offset;
    uint16_t right_offset;
    uint8_t writes_reference_constant;
} vf2_player_27564_plan;

static vf2_status player_27564_classify(
    uint32_t state,
    uint32_t c30,
    uint32_t player_flags,
    vf2_player_27564_plan *plan
)
{
    const unsigned player_side = (unsigned)((player_flags >> 6u) & 1u);
    const unsigned c30_side = (unsigned)((c30 >> 11u) & 1u);

    if (plan == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    if ((c30 & (UINT32_C(1) << 10u)) == 0u) {
        plan->outcome = VF2_PLAYER_27564_SKIP;
        return VF2_OK;
    }
    if ((state & (UINT32_C(1) << 10u)) != 0u) {
        plan->outcome = VF2_PLAYER_27564_EARLY_RETURN;
        return VF2_OK;
    }
    if ((state & (UINT32_C(1) << 9u)) == 0u) {
        plan->outcome = VF2_PLAYER_27564_SKIP;
        return VF2_OK;
    }

    plan->outcome = VF2_PLAYER_27564_APPLY;
    if (player_side == c30_side) {
        plan->left_offset = UINT16_C(0x01d0);
        plan->right_offset = UINT16_C(0x00e0);
        plan->writes_reference_constant = UINT8_C(1);
    } else {
        plan->left_offset = UINT16_C(0x01dc);
        plan->right_offset = UINT16_C(0x00ec);
    }
    return VF2_OK;
}
