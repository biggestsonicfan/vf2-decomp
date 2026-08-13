#define VF2_PLAYER_SELECTOR_SCRATCH_TABLE UINT32_C(0x02120004)
#define VF2_PLAYER_SELECTOR_DYNAMIC_TABLE UINT32_C(0x00029724)
#define VF2_PLAYER_SELECTOR_SCRATCH_RAW_COUNT 20u
#define VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT 60u
#define VF2_PLAYER_SELECTOR_SCRATCH_MAX_OPERATIONS 20u

typedef enum vf2_player_selector_scratch_operation_kind {
    VF2_PLAYER_SELECTOR_SCRATCH_COPY_TRIPLE = 0,
    VF2_PLAYER_SELECTOR_SCRATCH_STORE_SHORT3,
    VF2_PLAYER_SELECTOR_SCRATCH_STORE_SHORT4
} vf2_player_selector_scratch_operation_kind;

typedef struct vf2_player_selector_scratch_operation {
    vf2_player_selector_scratch_operation_kind kind;
    uint32_t target;
    uint32_t raw[3];
    uint16_t converted[4];
    uint8_t dynamic_bit;
} vf2_player_selector_scratch_operation;

typedef struct vf2_player_selector_scratch_plan {
    uint32_t player;
    uint32_t selector;
    uint32_t table_pointer;
    uint32_t scratch_base;
    uint32_t code_pointer_end;
    uint32_t aligned_count_pointer;
    uint32_t payload_start;
    uint32_t payload_end;
    uint8_t codes[VF2_PLAYER_SELECTOR_SCRATCH_CODE_COUNT];
    uint8_t dynamic_mask;
    size_t operation_count;
    vf2_player_selector_scratch_operation
        operations[VF2_PLAYER_SELECTOR_SCRATCH_MAX_OPERATIONS];
} vf2_player_selector_scratch_plan;
