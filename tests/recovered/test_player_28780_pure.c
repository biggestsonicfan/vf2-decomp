#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/status.h"

#include "../../src/recovered/player_28780_schedule.inc"
#include "../../src/recovered/player_28780_select.inc"
#include "../../src/recovered/player_28780_convert.inc"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static uint32_t float_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void test_word_selection(void)
{
    const uint32_t payload[4] = {
        UINT32_C(0x11111111), UINT32_C(0x22222222),
        UINT32_C(0x33333333), UINT32_C(0x44444444)
    };
    vf2_player_28780_action action;
    uint32_t selected = 0u;

    memset(&action, 0, sizeof(action));
    action.kind = VF2_PLAYER_28780_KEEP;
    CHECK(player_28780_select_word(
        &action, UINT32_C(0xdeadbeef), NULL, 0u, &selected
    ) == VF2_OK);
    CHECK(selected == UINT32_C(0xdeadbeef));

    action.kind = VF2_PLAYER_28780_ZERO;
    CHECK(player_28780_select_word(
        &action, UINT32_C(0xdeadbeef), NULL, 0u, &selected
    ) == VF2_OK);
    CHECK(selected == 0u);

    action.kind = VF2_PLAYER_28780_COPY;
    action.payload_offset = UINT32_C(8);
    CHECK(player_28780_select_word(
        &action, 0u, payload, 4u, &selected
    ) == VF2_OK);
    CHECK(selected == UINT32_C(0x33333333));

    action.payload_offset = UINT32_C(2);
    CHECK(player_28780_select_word(
        &action, 0u, payload, 4u, &selected
    ) == VF2_ERROR_UNSUPPORTED);

    action.payload_offset = UINT32_C(16);
    CHECK(player_28780_select_word(
        &action, 0u, payload, 4u, &selected
    ) == VF2_ERROR_UNSUPPORTED);
}

static void test_cvtri_preserves_high_half(void)
{
    uint32_t converted = 0u;
    const uint32_t one_half = float_bits(1.5f);

    CHECK(one_half == UINT32_C(0x3fc00000));
    CHECK(player_28780_cvtri_low16(
        0u, one_half, &converted
    ) == VF2_OK);
    CHECK(converted == UINT32_C(0x3fc00002));
    CHECK((converted & UINT32_C(0xffff0000)) ==
          (one_half & UINT32_C(0xffff0000)));
}

static void test_cvtri_rounding_modes(void)
{
    const uint32_t positive = float_bits(1.6f);
    const uint32_t negative = float_bits(-1.6f);
    uint32_t converted = 0u;

    CHECK(player_28780_cvtri_low16(
        0u, positive, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(2));
    CHECK(player_28780_cvtri_low16(
        UINT32_C(1) << 30u, positive, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(1));
    CHECK(player_28780_cvtri_low16(
        UINT32_C(2) << 30u, positive, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(2));
    CHECK(player_28780_cvtri_low16(
        UINT32_C(3) << 30u, positive, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(1));

    CHECK(player_28780_cvtri_low16(
        UINT32_C(1) << 30u, negative, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(0xfffe));
    CHECK(player_28780_cvtri_low16(
        UINT32_C(2) << 30u, negative, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(0xffff));
    CHECK(player_28780_cvtri_low16(
        UINT32_C(3) << 30u, negative, &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(0xffff));
}

static void test_cvtri_ties_and_invalid(void)
{
    uint32_t converted = 0u;

    CHECK(player_28780_cvtri_low16(
        0u, float_bits(2.5f), &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(2));
    CHECK(player_28780_cvtri_low16(
        0u, float_bits(3.5f), &converted
    ) == VF2_OK);
    CHECK((converted & UINT32_C(0xffff)) == UINT32_C(4));
    CHECK(player_28780_cvtri_low16(
        0u, UINT32_C(0x7fc00000), &converted
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(player_28780_cvtri_low16(
        0u, float_bits(1.0f), NULL
    ) == VF2_ERROR_UNSUPPORTED);
}

int main(void)
{
    test_word_selection();
    test_cvtri_preserves_high_half();
    test_cvtri_rounding_modes();
    test_cvtri_ties_and_invalid();

    if (failures != 0) {
        fprintf(stderr, "%d player 28780 pure test(s) failed\n", failures);
        return 1;
    }
    printf("player 28780 pure tests passed\n");
    return 0;
}
