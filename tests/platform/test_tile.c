/* Phase 7 tile-layer font extraction, validated against the committed
 * main_data ROM (decomp/i960/notes/tile_layer_glyph_format.md). ROM-independent
 * guards always run; the glyph checks run when a ROM directory is given. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/rom.h"
#include "vf2/tile.h"

static int failures = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);    \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

static int glyph_nonzero(const uint8_t *g)
{
    size_t i;
    for (i = 0u; i < VF2_TILE_GLYPH_PIXELS; ++i) {
        if (g[i] != 0u) {
            return 1;
        }
    }
    return 0;
}

static void test_guards(void)
{
    uint8_t g[VF2_TILE_GLYPH_PIXELS];
    static const uint8_t tiny[4] = { 0u, 0u, 0u, 0u };
    CHECK(vf2_tile_font_glyph(NULL, 0u, 0x41u, g) == VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(vf2_tile_font_glyph(tiny, sizeof(tiny), 0x41u, NULL) ==
          VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(vf2_tile_font_glyph(tiny, sizeof(tiny), 0x41u, g) ==
          VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(vf2_tile_name_char(0x8041u) == 0x41u);
    CHECK(vf2_tile_name_bank(0x8041u) == 0x80u);

    /* RGB555 palette decode (pure, synthetic known values from the attract
     * palette). */
    CHECK(vf2_tile_palette_rgb555(0xffffu) == 0xffffffu); /* white */
    CHECK(vf2_tile_palette_rgb555(0x8000u) == 0x000000u); /* black (flag only) */
    CHECK(vf2_tile_palette_rgb555(0xfc00u) == 0xff0000u); /* red  */
    CHECK(vf2_tile_palette_rgb555(0x83e0u) == 0x00ff00u); /* green */
    CHECK(vf2_tile_palette_rgb555(0x801fu) == 0x0000ffu); /* blue */
}

static void test_font(const char *rom_dir)
{
    uint8_t *main_data = NULL;
    size_t size = 0u;
    uint8_t space[VF2_TILE_GLYPH_PIXELS];
    uint8_t one[VF2_TILE_GLYPH_PIXELS];
    uint8_t aa[VF2_TILE_GLYPH_PIXELS];
    uint8_t ss[VF2_TILE_GLYPH_PIXELS];

    if (vf2_romset_build_region(rom_dir, VF2_REGION_MAIN_DATA,
                                &main_data, &size) != VF2_OK || main_data == NULL) {
        fprintf(stderr, "could not load main_data from %s\n", rom_dir);
        ++failures;
        return;
    }

    CHECK(vf2_tile_font_glyph(main_data, size, 0x20u, space) == VF2_OK);
    CHECK(vf2_tile_font_glyph(main_data, size, 0x31u, one) == VF2_OK);
    CHECK(vf2_tile_font_glyph(main_data, size, 0x41u, aa) == VF2_OK);
    CHECK(vf2_tile_font_glyph(main_data, size, 0x53u, ss) == VF2_OK);

    /* Space is blank; letters/digits are not; distinct characters differ.
     * These are the self-consistency invariants of an ASCII font table. */
    CHECK(!glyph_nonzero(space));
    CHECK(glyph_nonzero(one));
    CHECK(glyph_nonzero(aa));
    CHECK(glyph_nonzero(ss));
    CHECK(memcmp(one, aa, sizeof(one)) != 0);
    CHECK(memcmp(aa, ss, sizeof(aa)) != 0);
    /* Every pixel is a 4-bit index. */
    {
        size_t i;
        for (i = 0u; i < VF2_TILE_GLYPH_PIXELS; ++i) {
            CHECK(aa[i] <= 0x0fu);
        }
    }

    /* Row renderer: "SEGA" must blit each cell equal to that char's glyph. */
    {
        static const uint16_t sega[4] = {
            0x8053u, 0x8045u, 0x8047u, 0x8041u /* bank 0x80 | S,E,G,A */
        };
        uint8_t row[4u * VF2_TILE_GLYPH_W * VF2_TILE_GLYPH_H];
        uint8_t glyph[VF2_TILE_GLYPH_PIXELS];
        size_t col, x, y;
        CHECK(vf2_tile_render_row(main_data, size, sega, 4u, row) == VF2_OK);
        for (col = 0u; col < 4u; ++col) {
            CHECK(vf2_tile_font_glyph(main_data, size,
                                      (uint8_t)(sega[col] & 0xffu), glyph) == VF2_OK);
            for (y = 0u; y < VF2_TILE_GLYPH_H; ++y) {
                for (x = 0u; x < VF2_TILE_GLYPH_W; ++x) {
                    CHECK(row[y * (4u * VF2_TILE_GLYPH_W) + col * VF2_TILE_GLYPH_W + x]
                          == glyph[y * VF2_TILE_GLYPH_W + x]);
                }
            }
        }

        /* RGB composition: a synthetic 16-entry palette where index i maps to a
         * known colour; every output pixel must equal the decode of that
         * palette entry for the glyph's nibble. */
        {
            uint8_t pal[32];
            uint32_t rgb[4u * VF2_TILE_GLYPH_W * VF2_TILE_GLYPH_H];
            size_t i;
            for (i = 0u; i < 16u; ++i) {
                const uint16_t e = (uint16_t)(0x8000u | (i << 10) | (i << 5) | i);
                pal[i * 2u] = (uint8_t)(e & 0xffu);
                pal[i * 2u + 1u] = (uint8_t)(e >> 8);
            }
            CHECK(vf2_tile_render_row_rgb(main_data, size, sega, 4u,
                                          pal, sizeof(pal), 0u, rgb) == VF2_OK);
            for (col = 0u; col < 4u; ++col) {
                CHECK(vf2_tile_font_glyph(main_data, size,
                                          (uint8_t)(sega[col] & 0xffu), glyph) == VF2_OK);
                for (y = 0u; y < VF2_TILE_GLYPH_H; ++y) {
                    for (x = 0u; x < VF2_TILE_GLYPH_W; ++x) {
                        const uint8_t idx = glyph[y * VF2_TILE_GLYPH_W + x];
                        const uint16_t e =
                            (uint16_t)(0x8000u | (idx << 10) | (idx << 5) | idx);
                        CHECK(rgb[y * (4u * VF2_TILE_GLYPH_W) +
                                  col * VF2_TILE_GLYPH_W + x] ==
                              vf2_tile_palette_rgb555(e));
                    }
                }
            }
            /* Out-of-range palette base fails closed. */
            CHECK(vf2_tile_render_row_rgb(main_data, size, sega, 4u,
                                          pal, sizeof(pal), 1000u, rgb) ==
                  VF2_ERROR_OUT_OF_BOUNDS);
        }
    }

    free(main_data);
}

int main(int argc, char **argv)
{
    test_guards();
    if (argc >= 2) {
        test_font(argv[1]);
    }
    if (failures != 0) {
        fprintf(stderr, "%d tile test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("tile tests passed");
    return EXIT_SUCCESS;
}
