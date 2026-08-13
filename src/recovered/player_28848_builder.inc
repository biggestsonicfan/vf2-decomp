static vf2_status player_28848_build_schedule(
    const uint8_t codes[VF2_PLAYER_28848_CODE_COUNT],
    const uint8_t *counts,
    size_t count_count,
    vf2_player_28848_schedule *schedule
)
{
    uint32_t input_offset = 0u;
    size_t count_index = 0u;
    size_t index = 0u;

    if (codes == NULL || schedule == NULL ||
        (counts == NULL && count_count != 0u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(schedule, 0, sizeof(*schedule));

    for (index = 0u; index < VF2_PLAYER_28848_CODE_COUNT; ++index) {
        vf2_player_28848_step *step = &schedule->steps[index];
        const uint8_t code = codes[index];
        uint32_t advance = 0u;

        step->code = code;
        step->input_offset_before = input_offset;
        step->source_offset = input_offset;
        if (code <= UINT8_C(2)) {
            step->action = VF2_PLAYER_28848_KEEP;
            advance = UINT32_C(4);
        } else if (code == UINT8_C(3)) {
            step->action = VF2_PLAYER_28848_ZERO;
        } else if (code == UINT8_C(4)) {
            step->action = VF2_PLAYER_28848_COPY;
            advance = UINT32_C(4);
        } else if (code == UINT8_C(5) || code == UINT8_C(6)) {
            const uint32_t stride = code == UINT8_C(5)
                ? UINT32_C(4) : UINT32_C(12);
            uint32_t count = 0u;

            if (count_index >= count_count) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            count = (uint32_t)counts[count_index++];
            advance = count * stride;
            step->action = VF2_PLAYER_28848_COPY;
            step->source_offset = input_offset + advance - stride;
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        input_offset += advance;
        step->input_offset_after = input_offset;
    }
    schedule->count_values_used = count_index;
    schedule->final_input_offset = input_offset;
    return VF2_OK;
}
