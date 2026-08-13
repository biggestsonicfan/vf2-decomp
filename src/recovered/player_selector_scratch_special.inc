static vf2_status player_selector_scratch_plan_special(
    const vf2_model2a *machine,
    vf2_player_selector_scratch_plan *plan,
    uint32_t arithmetic_control,
    size_t outer,
    uint8_t code,
    uint32_t *payload
)
{
    vf2_player_selector_scratch_operation *operation = NULL;
    uint32_t target_offset = 0u;
    uint32_t target = 0u;
    uint32_t raw = 0u;
    uint32_t index = 0u;
    size_t component = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL || payload == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = player_selector_scratch_push_operation(plan, &operation);
    if (status != VF2_OK) {
        return status;
    }

    if (code == 0u) {
        status = player_selector_scratch_add(
            plan->player, UINT32_C(0x80), &operation->target
        );
        if (status != VF2_OK) {
            return status;
        }
        operation->kind = VF2_PLAYER_SELECTOR_SCRATCH_COPY_TRIPLE;
        for (component = 0u; component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine,
                *payload + (uint32_t)(component * 4u),
                &operation->raw[component]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        status = player_selector_scratch_validate_write(
            machine, operation->target, 12u
        );
        if (status != VF2_OK) {
            return status;
        }
        return player_selector_scratch_advance(payload, 1u, 12u);
    }

    if (outer < 12u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    index = (uint32_t)(outer - 12u);
    if (index >= 8u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine,
        VF2_PLAYER_SELECTOR_DYNAMIC_TABLE + index * UINT32_C(4),
        &target_offset
    );
    if (status != VF2_OK) {
        return status;
    }
    status = player_selector_scratch_add(
        plan->player, target_offset, &target
    );
    if (status != VF2_OK) {
        return status;
    }
    operation->target = target;
    operation->dynamic_bit = (uint8_t)(UINT8_C(1) << index);
    plan->dynamic_mask =
        (uint8_t)(plan->dynamic_mask | operation->dynamic_bit);

    if (code == 1u) {
        operation->kind = VF2_PLAYER_SELECTOR_SCRATCH_STORE_SHORT3;
        for (component = 0u; component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine,
                *payload + (uint32_t)(component * 4u),
                &raw
            );
            if (status == VF2_OK) {
                status = player_selector_scratch_cvtri(
                    arithmetic_control, raw, &operation->converted[component]
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        status = player_selector_scratch_validate_write(
            machine, operation->target, 6u
        );
        if (status != VF2_OK) {
            return status;
        }
        return player_selector_scratch_advance(payload, 1u, 12u);
    }

    if (code != 2u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    operation->kind = VF2_PLAYER_SELECTOR_SCRATCH_STORE_SHORT4;
    status = vf2_model2a_read_u32(machine, *payload, &raw);
    if (status == VF2_OK) {
        status = player_selector_scratch_cvtri(
            arithmetic_control, raw, &operation->converted[0]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = player_selector_scratch_advance(payload, 1u, 4u);
    if (status != VF2_OK) {
        return status;
    }
    for (component = 1u; component < 4u; ++component) {
        status = vf2_model2a_read_u32(
            machine,
            *payload + (uint32_t)(component * 4u),
            &raw
        );
        if (status == VF2_OK) {
            status = player_selector_scratch_cvtri(
                arithmetic_control, raw, &operation->converted[component]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }
    status = player_selector_scratch_validate_write(
        machine, operation->target, 8u
    );
    if (status != VF2_OK) {
        return status;
    }
    return player_selector_scratch_advance(payload, 1u, 16u);
}
