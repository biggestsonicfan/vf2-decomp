static vf2_status player_selector_scratch_interpret_codes(
    const vf2_model2a *machine,
    vf2_player_selector_scratch_plan *plan,
    uint32_t arithmetic_control,
    uint32_t count_pointer,
    uint32_t payload
)
{
    size_t code_index = 0u;
    size_t outer = 0u;
    size_t inner = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    for (outer = 0u; outer < 20u; ++outer) {
        int special = 0;

        for (inner = 0u; inner < 3u; ++inner) {
            uint8_t code = 0u;

            if (code_index >= VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT) {
                return VF2_ERROR_UNSUPPORTED;
            }
            code = plan->codes[code_index];
            if (code > UINT8_C(5)) {
                uint8_t count = 0u;
                status = player_selector_scratch_read_u8(
                    machine, count_pointer, &count
                );
                if (status == VF2_OK) {
                    if (count_pointer == UINT32_MAX) {
                        return VF2_ERROR_UNSUPPORTED;
                    }
                    ++count_pointer;
                    status = player_selector_scratch_advance(
                        &payload, (uint32_t)count, UINT32_C(12)
                    );
                }
            } else if (code == UINT8_C(5)) {
                uint8_t count = 0u;
                status = player_selector_scratch_read_u8(
                    machine, count_pointer, &count
                );
                if (status == VF2_OK) {
                    if (count_pointer == UINT32_MAX) {
                        return VF2_ERROR_UNSUPPORTED;
                    }
                    ++count_pointer;
                    status = player_selector_scratch_advance(
                        &payload, (uint32_t)count, UINT32_C(4)
                    );
                }
            } else if (code == UINT8_C(4)) {
                uint32_t discarded = 0u;
                status = vf2_model2a_read_u32(machine, payload, &discarded);
                if (status == VF2_OK) {
                    status = player_selector_scratch_advance(
                        &payload, 1u, UINT32_C(4)
                    );
                }
            } else if (code <= UINT8_C(2)) {
                status = player_selector_scratch_plan_special(
                    machine, plan, arithmetic_control, outer, code, &payload
                );
                special = 1;
            }
            if (status != VF2_OK) {
                return status;
            }
            if (special) {
                code_index = (outer + 1u) * 3u;
                break;
            }
            ++code_index;
        }
    }
    if (code_index != VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT) {
        return VF2_ERROR_UNSUPPORTED;
    }
    plan->payload_end = payload;
    return VF2_OK;
}
