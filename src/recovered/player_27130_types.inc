typedef enum vf2_player_27130_outcome {
    VF2_PLAYER_27130_EARLY_RETURN = 0,
    VF2_PLAYER_27130_MODE2_RETURN,
    VF2_PLAYER_27130_HEAVY_CONTINUATION
} vf2_player_27130_outcome;

typedef struct vf2_player_27130_facts {
    uint8_t global_gate;
    uint32_t player_flags;
    uint8_t previous_be4;
    uint16_t selector_value;
    uint32_t state;
    uint32_t selector_state;
    uint32_t old_selector_state;
    uint8_t selector_byte;
    uint8_t threshold_enabled;
    uint16_t threshold;
    uint8_t previous_be5;
    uint8_t previous_bdd;
} vf2_player_27130_facts;

typedef struct vf2_player_27130_plan {
    vf2_player_27130_outcome outcome;
    uint8_t mode;
    uint16_t selector_value;
    uint16_t bucket;
    uint32_t remainder;
} vf2_player_27130_plan;
