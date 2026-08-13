static vf2_status player_28780_select_word(
    const vf2_player_28780_action *action,
    uint32_t initial_word,
    const uint32_t *payload_words,
    size_t payload_word_count,
    uint32_t *selected
)
{
    size_t payload_index = 0u;

    if (action == NULL || selected == NULL ||
        (payload_word_count != 0u && payload_words == NULL)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (action->kind == VF2_PLAYER_28780_KEEP) {
        *selected = initial_word;
        return VF2_OK;
    }
    if (action->kind == VF2_PLAYER_28780_ZERO) {
        *selected = 0u;
        return VF2_OK;
    }
    if (action->kind != VF2_PLAYER_28780_COPY ||
        (action->payload_offset & UINT32_C(3)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    payload_index = (size_t)(action->payload_offset / UINT32_C(4));
    if (payload_index >= payload_word_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *selected = payload_words[payload_index];
    return VF2_OK;
}
