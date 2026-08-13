static vf2_status player_selector_scratch_preflight(
    const vf2_model2a *machine,
    vf2_player_selector_scratch_plan *plan,
    uint32_t arithmetic_control,
    uint32_t count_pointer_start
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (plan->selector == 0u) {
        return VF2_OK;
    }

    status = player_selector_scratch_interpret_codes(
        machine,
        plan,
        arithmetic_control,
        count_pointer_start,
        plan->payload_start
    );
    if (status != VF2_OK) {
        return status;
    }

    status = player_selector_scratch_validate_write(
        machine,
        plan->scratch_base + UINT32_C(0x780),
        UINT32_C(12)
    );
    if (status == VF2_OK) {
        status = player_selector_scratch_validate_write(
            machine,
            plan->scratch_base + UINT32_C(0x78c),
            VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT
        );
    }
    if (status == VF2_OK) {
        status = player_selector_scratch_validate_write(
            machine, plan->player + UINT32_C(0xbdc), 1u
        );
    }
    return status;
}
