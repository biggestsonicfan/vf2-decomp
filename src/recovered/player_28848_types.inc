#define VF2_PLAYER_28848_CODE_COUNT 60u

typedef enum vf2_player_28848_action {
    VF2_PLAYER_28848_KEEP = 0,
    VF2_PLAYER_28848_ZERO,
    VF2_PLAYER_28848_COPY
} vf2_player_28848_action;

typedef struct vf2_player_28848_step {
    vf2_player_28848_action action;
    uint8_t code;
    uint32_t input_offset_before;
    uint32_t input_offset_after;
    uint32_t source_offset;
} vf2_player_28848_step;

typedef struct vf2_player_28848_schedule {
    size_t count_values_used;
    uint32_t final_input_offset;
    vf2_player_28848_step steps[VF2_PLAYER_28848_CODE_COUNT];
} vf2_player_28848_schedule;
