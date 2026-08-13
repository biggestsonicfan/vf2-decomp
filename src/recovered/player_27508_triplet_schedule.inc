#define VF2_PLAYER_27508_VECTOR_COUNT 8u
#define VF2_PLAYER_27508_WORDS_PER_VECTOR 3u

typedef struct vf2_player_27508_operation {
    uint8_t vector_index;
    uint32_t input_x_bits;
    uint32_t input_y_bits;
    uint32_t input_z_bits;
    uint8_t output_x_index;
    uint8_t output_z_index;
} vf2_player_27508_operation;

typedef struct vf2_player_27508_schedule {
    size_t operation_count;
    vf2_player_27508_operation operations[VF2_PLAYER_27508_VECTOR_COUNT];
} vf2_player_27508_schedule;

static vf2_status player_27508_build_triplet_schedule(
    const uint32_t group[
        VF2_PLAYER_27508_VECTOR_COUNT * VF2_PLAYER_27508_WORDS_PER_VECTOR
    ],
    vf2_player_27508_schedule *schedule
)
{
    size_t vector = 0u;

    if (group == NULL || schedule == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(schedule, 0, sizeof(*schedule));
    for (vector = 0u; vector < VF2_PLAYER_27508_VECTOR_COUNT; ++vector) {
        vf2_player_27508_operation *operation =
            &schedule->operations[schedule->operation_count++];
        const size_t base = vector * VF2_PLAYER_27508_WORDS_PER_VECTOR;

        operation->vector_index = (uint8_t)vector;
        operation->input_x_bits = group[base];
        operation->input_y_bits = 0u;
        operation->input_z_bits = group[base + 2u];
        operation->output_x_index = 0u;
        operation->output_z_index = 2u;
    }
    return VF2_OK;
}
