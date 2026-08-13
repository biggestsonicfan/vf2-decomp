#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/status.h"

#include "../../src/recovered/player_i960_bridge_planar_rotation.inc"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static uint32_t bits(float value)
{
    uint32_t result = 0u;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float value(uint32_t raw)
{
    float result = 0.0f;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

static void check_rotation(
    int16_t angle,
    float x,
    float z,
    float expected_x,
    float expected_z
)
{
    uint32_t output_x = 0u;
    uint32_t output_z = 0u;

    CHECK(player_planar_rotation_apply(
        angle, bits(x), bits(z), &output_x, &output_z
    ) == VF2_OK);
    CHECK(value(output_x) == expected_x);
    CHECK(value(output_z) == expected_z);
}

static void test_cardinal_orientation(void)
{
    check_rotation(0, 2.0f, 3.0f, 2.0f, 3.0f);

    /* Positive quarter turn is clockwise in X/Z: +X -> -Z. */
    check_rotation((int16_t)UINT16_C(0x4000), 2.0f, 3.0f, 3.0f, -2.0f);

    /* Negative quarter turn is the inverse: +X -> +Z. */
    check_rotation((int16_t)UINT16_C(0xc000), 2.0f, 3.0f, -3.0f, 2.0f);

    check_rotation((int16_t)UINT16_C(0x8000), 2.0f, 3.0f, -2.0f, -3.0f);
}

static void test_atan_edge_alignment_evidence(void)
{
    uint32_t output_x = 0u;
    uint32_t output_z = 0u;
    const float one = 1.0f;

    /* For an edge (dx,dz)=(1,1), atan2(dz,dx)=0x2000. The ROM adds
     * 0x4000 before command 0x2d805b5b. Under the recovered clockwise matrix
     * this makes the transverse component exactly zero, matching the min/max
     * alignment purpose of the 0x3a320 call site. */
    CHECK(player_planar_rotation_apply(
        (int16_t)UINT16_C(0x6000), bits(one), bits(one),
        &output_x, &output_z
    ) == VF2_OK);
    CHECK(value(output_x) == 0.0f);
    CHECK(value(output_z) < -1.4141f && value(output_z) > -1.4143f);
}

static void test_nonfinite_rejection(void)
{
    uint32_t output_x = 0u;
    uint32_t output_z = 0u;

    CHECK(player_planar_rotation_apply(
        0, UINT32_C(0x7f800000), bits(1.0f), &output_x, &output_z
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(player_planar_rotation_apply(
        0, bits(1.0f), UINT32_C(0x7fc00000), &output_x, &output_z
    ) == VF2_ERROR_UNSUPPORTED);
}

int main(void)
{
    test_cardinal_orientation();
    test_atan_edge_alignment_evidence();
    test_nonfinite_rejection();

    if (failures != 0) {
        fprintf(stderr, "%d player planar rotation test(s) failed\n", failures);
        return 1;
    }
    printf("player planar rotation semantics tests passed\n");
    return 0;
}
