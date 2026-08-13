#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/status.h"

#include "../../src/recovered/player_28780_schedule.inc"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void test_zero_codes(void)
{
    uint8_t codes[VF2_PLAYER_28780_SLOT_COUNT];
    vf2_player_28780_schedule schedule;
    size_t index = 0u;

    memset(codes, UINT8_C(3), sizeof(codes));
    CHECK(player_28780_schedule(codes, NULL, 0u, &schedule) == VF2_OK);
    CHECK(schedule.counts_consumed == 0u);
    CHECK(schedule.payload_bytes_consumed == 0u);
    for (index = 0u; index < VF2_PLAYER_28780_SLOT_COUNT; ++index) {
        CHECK(schedule.actions[index].kind == VF2_PLAYER_28780_ZERO);
        CHECK(schedule.actions[index].payload_offset == 0u);
    }
}

static void test_mixed_codes(void)
{
    uint8_t codes[VF2_PLAYER_28780_SLOT_COUNT];
    const uint8_t counts[2] = {UINT8_C(2), UINT8_C(3)};
    vf2_player_28780_schedule schedule;

    memset(codes, UINT8_C(3), sizeof(codes));
    codes[0] = UINT8_C(0);
    codes[1] = UINT8_C(1);
    codes[2] = UINT8_C(2);
    codes[3] = UINT8_C(3);
    codes[4] = UINT8_C(4);
    codes[5] = UINT8_C(5);
    codes[6] = UINT8_C(6);

    CHECK(player_28780_schedule(
        codes, counts, 2u, &schedule
    ) == VF2_OK);
    CHECK(schedule.actions[0].kind == VF2_PLAYER_28780_KEEP);
    CHECK(schedule.actions[0].payload_offset == UINT32_C(0));
    CHECK(schedule.actions[1].kind == VF2_PLAYER_28780_KEEP);
    CHECK(schedule.actions[1].payload_offset == UINT32_C(4));
    CHECK(schedule.actions[2].kind == VF2_PLAYER_28780_KEEP);
    CHECK(schedule.actions[2].payload_offset == UINT32_C(8));
    CHECK(schedule.actions[3].kind == VF2_PLAYER_28780_ZERO);
    CHECK(schedule.actions[3].payload_offset == UINT32_C(12));
    CHECK(schedule.actions[4].kind == VF2_PLAYER_28780_COPY);
    CHECK(schedule.actions[4].payload_offset == UINT32_C(12));
    CHECK(schedule.actions[5].kind == VF2_PLAYER_28780_COPY);
    CHECK(schedule.actions[5].payload_offset == UINT32_C(16));
    CHECK(schedule.actions[6].kind == VF2_PLAYER_28780_COPY);
    CHECK(schedule.actions[6].payload_offset == UINT32_C(24));
    CHECK(schedule.actions[7].payload_offset == UINT32_C(60));
    CHECK(schedule.counts_consumed == 2u);
    CHECK(schedule.payload_bytes_consumed == UINT32_C(60));
}

static void test_count_zero_and_errors(void)
{
    uint8_t codes[VF2_PLAYER_28780_SLOT_COUNT];
    const uint8_t zero_count[1] = {0u};
    vf2_player_28780_schedule schedule;

    memset(codes, UINT8_C(3), sizeof(codes));
    codes[0] = UINT8_C(5);
    codes[1] = UINT8_C(4);
    CHECK(player_28780_schedule(
        codes, zero_count, 1u, &schedule
    ) == VF2_OK);
    CHECK(schedule.actions[0].payload_offset == 0u);
    CHECK(schedule.actions[1].payload_offset == 0u);
    CHECK(schedule.payload_bytes_consumed == UINT32_C(4));

    memset(codes, UINT8_C(3), sizeof(codes));
    codes[0] = UINT8_C(6);
    CHECK(player_28780_schedule(
        codes, NULL, 0u, &schedule
    ) == VF2_ERROR_UNSUPPORTED);

    memset(codes, UINT8_C(3), sizeof(codes));
    codes[0] = UINT8_C(7);
    CHECK(player_28780_schedule(
        codes, NULL, 0u, &schedule
    ) == VF2_ERROR_UNSUPPORTED);
}

int main(void)
{
    test_zero_codes();
    test_mixed_codes();
    test_count_zero_and_errors();

    if (failures != 0) {
        fprintf(stderr, "%d player 28780 schedule test(s) failed\n", failures);
        return 1;
    }
    printf("player 28780 schedule tests passed\n");
    return 0;
}
