static vf2_status player_selector_scratch_expand_codes(
    const vf2_model2a *machine,
    vf2_player_selector_scratch_plan *plan,
    uint32_t raw_pointer,
    uint32_t count_pointer_start,
    uint32_t *count_pointer_end,
    uint32_t *count_sum_out,
    size_t *count_records_out
)
{
    uint32_t count_pointer = count_pointer_start;
    uint32_t count_sum = 0u;
    size_t count_records = 0u;
    size_t code_index = 0u;
    size_t outer = 0u;
    size_t inner = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL || count_pointer_end == NULL ||
        count_sum_out == NULL || count_records_out == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    for (outer = 0u; outer < VF2_PLAYER_SELECTOR_SCRATCH_RAW_COUNT; ++outer) {
        uint8_t raw = 0u;
        const uint8_t mode_mask = UINT8_C(3);

        status = player_selector_scratch_read_u8(
            machine, raw_pointer + (uint32_t)outer, &raw
        );
        if (status != VF2_OK) {
            return status;
        }
        for (inner = 0u; inner < 3u; ++inner) {
            const uint8_t mode =
                (uint8_t)((raw >> 6u) & mode_mask);
            uint8_t code = 0u;

            if (mode != 0u) {
                code = (uint8_t)(mode - UINT8_C(1));
            } else {
                code = (uint8_t)(
                    ((raw >> (uint32_t)(inner * 2u)) & mode_mask) +
                    UINT8_C(3)
                );
            }
            if (code_index >= VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT) {
                return VF2_ERROR_UNSUPPORTED;
            }
            plan->codes[code_index++] = code;
            if (code > UINT8_C(4)) {
                uint8_t count = 0u;
                status = player_selector_scratch_read_u8(
                    machine, count_pointer, &count
                );
                if (status != VF2_OK) {
                    return status;
                }
                if (count_sum > UINT32_MAX - (uint32_t)count ||
                    count_pointer == UINT32_MAX) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                count_sum += (uint32_t)count;
                ++count_pointer;
                ++count_records;
            }
        }
    }
    if (code_index != VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *count_pointer_end = count_pointer;
    *count_sum_out = count_sum;
    *count_records_out = count_records;
    return VF2_OK;
}
