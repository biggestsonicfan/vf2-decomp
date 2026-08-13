static vf2_status player_27130_classify(
    const vf2_player_27130_facts *facts,
    vf2_player_27130_plan *plan
)
{
    uint8_t mode = 0u;

    if (facts == NULL || plan == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    plan->outcome = VF2_PLAYER_27130_EARLY_RETURN;

    if (facts->global_gate != 0u ||
        (facts->player_flags & (UINT32_C(1) << 26u)) == 0u ||
        facts->previous_be4 == UINT8_C(1)) {
        return VF2_OK;
    }

    plan->selector_value = facts->selector_value;
    if ((facts->state & (UINT32_C(1) << 17u)) != 0u) {
        mode = UINT8_C(2);
    } else if ((facts->selector_state & (UINT32_C(3) << 18u)) != 0u) {
        mode = UINT8_C(1);
    } else {
        if ((facts->old_selector_state & (UINT32_C(1) << 23u)) != 0u ||
            (facts->old_selector_state & UINT32_C(0x00008040)) ==
                UINT32_C(0x00008040) ||
            facts->selector_byte == UINT8_C(4)) {
            return VF2_OK;
        }
        mode = facts->selector_byte == UINT8_C(0x7f) ||
               (facts->state & UINT32_C(0x00006600)) != 0u
            ? UINT8_C(1) : UINT8_C(3);
    }
    plan->mode = mode;

    if (facts->selector_value <= UINT16_C(2)) {
        return VF2_OK;
    }

    /* 0x271f4, 0x271f8 and 0x271fc are consecutive MOVs with no branches
     * between them.  Entering any of those labels therefore leaves r8=4. */
    plan->bucket = facts->selector_value <= UINT16_C(16)
        ? UINT16_C(4) : UINT16_C(8);
    plan->remainder =
        (uint32_t)facts->selector_value - (uint32_t)plan->bucket;

    if (facts->threshold_enabled != 0u &&
        plan->remainder < (uint32_t)facts->threshold) {
        mode = (uint8_t)(mode & UINT8_C(0xfd));
    }
    if ((facts->previous_be5 & UINT8_C(2)) != 0u &&
        (facts->previous_bdd & UINT8_C(2)) != 0u) {
        mode = (uint8_t)(mode & UINT8_C(0xfe));
    }
    plan->mode = mode;
    plan->outcome = mode == UINT8_C(2)
        ? VF2_PLAYER_27130_MODE2_RETURN
        : VF2_PLAYER_27130_HEAVY_CONTINUATION;
    return VF2_OK;
}
