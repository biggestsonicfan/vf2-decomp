#define VF2_PLAYER_27244_VECTOR_COUNT 20u
#define VF2_PLAYER_27244_WORD_COUNT 60u

typedef struct vf2_player_27244_exchange_result {
    uint32_t backup[VF2_PLAYER_27244_WORD_COUNT];
    uint32_t active[VF2_PLAYER_27244_WORD_COUNT];
} vf2_player_27244_exchange_result;

static vf2_status player_27244_exchange(
    const uint32_t scratch_before[VF2_PLAYER_27244_WORD_COUNT],
    const uint32_t expanded[VF2_PLAYER_27244_WORD_COUNT],
    vf2_player_27244_exchange_result *result
)
{
    if (scratch_before == NULL || expanded == NULL || result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memcpy(result->backup, scratch_before, sizeof(result->backup));
    memcpy(result->active, expanded, sizeof(result->active));
    return VF2_OK;
}
