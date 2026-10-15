/* Texture-sheet decoder tests. Synthetic data only -- no ROM, no capture, so
 * this runs in CI. Validates the texel bit-order (word = low16|high16, each
 * half four 4-bit texels MSB-nibble-first), the grayscale scaling, and the
 * fail-closed guards. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/texture_sheet.h"

static int failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);  \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void put_u32(uint8_t *p, size_t word, uint32_t v)
{
    p[word * 4u + 0u] = (uint8_t)(v & 0xffu);
    p[word * 4u + 1u] = (uint8_t)((v >> 8) & 0xffu);
    p[word * 4u + 2u] = (uint8_t)((v >> 16) & 0xffu);
    p[word * 4u + 3u] = (uint8_t)((v >> 24) & 0xffu);
}

static void test_bit_order(void)
{
    /* One 8-texel row. word = 0xHHHHLLLL with LLLL the low (first) half.
     * Choose low=0x1234, high=0x5678 -> texels lo:1,2,3,4 then hi:5,6,7,8. */
    uint8_t bank[4];
    uint8_t out[8];
    size_t n = 0u;
    put_u32(bank, 0u, 0x56781234u);

    CHECK(vf2_texture_sheet_decode(bank, sizeof(bank), 8u, 1u, out, sizeof(out),
                                   &n) == VF2_OK);
    CHECK(n == 8u);
    CHECK(out[0] == 0x1u && out[1] == 0x2u && out[2] == 0x3u && out[3] == 0x4u);
    CHECK(out[4] == 0x5u && out[5] == 0x6u && out[6] == 0x7u && out[7] == 0x8u);
}

static void test_two_rows(void)
{
    /* 8 wide x 2 tall: row0 word 0x00000000, row1 word 0xffffffff.
     * Row-major output: row0 all 0, row1 all 15. */
    uint8_t bank[8];
    uint8_t out[16];
    size_t n = 0u;
    size_t i;
    put_u32(bank, 0u, 0x00000000u);
    put_u32(bank, 1u, 0xffffffffu);

    CHECK(vf2_texture_sheet_decode(bank, sizeof(bank), 8u, 2u, out, sizeof(out),
                                   &n) == VF2_OK);
    CHECK(n == 16u);
    for (i = 0u; i < 8u; ++i) {
        CHECK(out[i] == 0u);
    }
    for (i = 8u; i < 16u; ++i) {
        CHECK(out[i] == 15u);
    }
}

static void test_gray_scale(void)
{
    CHECK(vf2_texture_sheet_index_to_gray(0u) == 0u);
    CHECK(vf2_texture_sheet_index_to_gray(1u) == 17u);
    CHECK(vf2_texture_sheet_index_to_gray(15u) == 255u);
    CHECK(vf2_texture_sheet_index_to_gray(16u) == 0u); /* out of range */
}

static void test_guards(void)
{
    uint8_t bank[8];
    uint8_t out[16];
    size_t n = 123u;
    memset(bank, 0, sizeof(bank));

    CHECK(vf2_texture_sheet_decode(NULL, 8u, 8u, 1u, out, sizeof(out), &n) ==
          VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_texture_sheet_decode(bank, sizeof(bank), 8u, 1u, NULL, sizeof(out),
                                   &n) == VF2_ERROR_INVALID_ARGUMENT);
    /* width not a multiple of 8 */
    CHECK(vf2_texture_sheet_decode(bank, sizeof(bank), 7u, 1u, out, sizeof(out),
                                   &n) == VF2_ERROR_INVALID_ARGUMENT);
    /* zero height */
    CHECK(vf2_texture_sheet_decode(bank, sizeof(bank), 8u, 0u, out, sizeof(out),
                                   &n) == VF2_ERROR_INVALID_ARGUMENT);
    /* bank too small: 8x2 needs 8 bytes, give 4 */
    CHECK(vf2_texture_sheet_decode(bank, 4u, 8u, 2u, out, sizeof(out), &n) ==
          VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(n == 0u);
    /* out too small: 8x1 needs 8 texels, give 4 */
    CHECK(vf2_texture_sheet_decode(bank, sizeof(bank), 8u, 1u, out, 4u, &n) ==
          VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(n == 0u);
}

int main(void)
{
    test_bit_order();
    test_two_rows();
    test_gray_scale();
    test_guards();
    if (failures != 0) {
        fprintf(stderr, "texture-sheet tests FAILED: %d\n", failures);
        return 1;
    }
    printf("texture-sheet tests passed\n");
    return 0;
}
