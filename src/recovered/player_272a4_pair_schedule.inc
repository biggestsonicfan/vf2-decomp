#define VF2_PLAYER_272A4_VECTOR_COUNT 8u
#define VF2_PLAYER_272A4_WORDS_PER_VECTOR 3u
#define VF2_PLAYER_272A4_MAX_OPERATIONS 16u

typedef struct vf2_player_272a4_operation {
    uint8_t vector_index;
    uint8_t component_index;
    uint32_t source_bits;
    uint32_t reference_bits;
} vf2_player_272a4_operation;

typedef struct vf2_player_272a4_schedule {
    size_t operation_count;
    vf2_player_272a4_operation operations[VF2_PLAYER_272A4_MAX_OPERATIONS];
} vf2_player_272a4_schedule;

static vf2_status player_272a4_build_pair_schedule(
    const uint32_t group[
        VF2_PLAYER_272A4_VECTOR_COUNT * VF2_PLAYER_272A4_WORDS_PER_VECTOR
    ],
    vf2_player_272a4_schedule *schedule
)
{
    size_t vector = 0u;

    if (group == NULL || schedule == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(schedule, 0, sizeof(*schedule));
    if ((group[0] | group[2]) == 0u) {
        return VF2_OK;
    }
    for (vector = 0u; vector < VF2_PLAYER_272A4_VECTOR_COUNT; ++vector) {
        vf2_player_272a4_operation *x =
            &schedule->operations[schedule->operation_count++];
        vf2_player_272a4_operation *z =
            &schedule->operations[schedule->operation_count++];
        const size_t base = vector * VF2_PLAYER_272A4_WORDS_PER_VECTOR;

        x->vector_index = (uint8_t)vector;
        x->component_index = 0u;
        x->source_bits = group[base];
        x->reference_bits = group[0];

        z->vector_index = (uint8_t)vector;
        z->component_index = 2u;
        z->source_bits = group[base + 2u];
        z->reference_bits = group[2];
    }
    return VF2_OK;
}
