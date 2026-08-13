static vf2_status player_selector_scratch_apply_fixed(
    vf2_model2a *machine,
    const vf2_player_selector_scratch_plan *plan
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL || plan->player == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (plan->selector == 0u) {
        return VF2_OK;
    }

    status = vf2_model2a_write_u32(
        machine,
        plan->scratch_base + UINT32_C(0x784),
        plan->code_pointer_end
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            plan->scratch_base + UINT32_C(0x788),
            plan->aligned_count_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            plan->scratch_base + UINT32_C(0x780),
            plan->payload_start
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            plan->scratch_base + UINT32_C(0x78c),
            plan->codes,
            sizeof(plan->codes)
        );
    }
    if (status == VF2_OK) {
        status = player_selector_scratch_write_u8(
            machine, plan->player + UINT32_C(0xbdc), 0u
        );
    }
    return status;
}
