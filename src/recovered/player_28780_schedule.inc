#define VF2_PLAYER_28780_SLOT_COUNT 60u

typedef enum vf2_player_28780_action_kind {
    VF2_PLAYER_28780_KEEP = 0,
    VF2_PLAYER_28780_ZERO,
    VF2_PLAYER_28780_COPY
} vf2_player_28780_action_kind;

typedef struct vf2_player_28780_action {
    vf2_player_28780_action_kind kind;
    uint32_t payload_offset;
} vf2_player_28780_action;

typedef struct vf2_player_28780_schedule {
    vf2_player_28780_action actions[VF2_PLAYER_28780_SLOT_COUNT];
    size_t counts_consumed;
    uint32_t payload_bytes_consumed;
} vf2_player_28780_schedule;

static vf2_status player_28780_schedule(
    const uint8_t codes[VF2_PLAYER_28780_SLOT_COUNT],
    const uint8_t *counts,
    size_t count_count,
    vf2_player_28780_schedule *schedule
)
{
    uint32_t payload = 0u;
    size_t count_index = 0u;
    size_t index = 0u;

    if (codes == NULL || schedule == NULL ||
        (count_count != 0u && counts == NULL)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(schedule, 0, sizeof(*schedule));

    for (index = 0u; index < VF2_PLAYER_28780_SLOT_COUNT; ++index) {
        const uint8_t code = codes[index];
        vf2_player_28780_action *action = &schedule->actions[index];
        uint32_t advance = 0u;

        action->payload_offset = payload;
        if (code == UINT8_C(3)) {
            action->kind = VF2_PLAYER_28780_ZERO;
            continue;
        }
        if (code <= UINT8_C(2)) {
            action->kind = VF2_PLAYER_28780_KEEP;
            advance = UINT32_C(4);
        } else if (code == UINT8_C(4)) {
            action->kind = VF2_PLAYER_28780_COPY;
            advance = UINT32_C(4);
        } else if (code == UINT8_C(5) || code == UINT8_C(6)) {
            uint32_t stride = 0u;

            if (count_index >= count_count) {
                return VF2_ERROR_UNSUPPORTED;
            }
            action->kind = VF2_PLAYER_28780_COPY;
            stride = code == UINT8_C(5) ? UINT32_C(4) : UINT32_C(12);
            if ((uint64_t)counts[count_index] * (uint64_t)stride > UINT32_MAX) {
                return VF2_ERROR_UNSUPPORTED;
            }
            advance = (uint32_t)counts[count_index] * stride;
            ++count_index;
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (payload > UINT32_MAX - advance) {
            return VF2_ERROR_UNSUPPORTED;
        }
        payload += advance;
    }

    schedule->counts_consumed = count_index;
    schedule->payload_bytes_consumed = payload;
    return VF2_OK;
}
