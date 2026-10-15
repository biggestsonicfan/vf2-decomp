/* Phase 7 tile-layer font extraction, validated against the committed
 * main_data ROM (decomp/i960/notes/tile_layer_glyph_format.md). ROM-independent
 * guards always run; the glyph checks run when a ROM directory is given. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/rom.h"
#include "vf2/session.h"
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
                            (uint16_t)(0x8000u | ((unsigned)idx << 10) |
                                       ((unsigned)idx << 5) | idx);
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

            /* Layer render over a synthetic 2-row tile-RAM with "HI" at (0,0). */
            {
                uint8_t tram[2u * VF2_TILE_NAME_STRIDE * 2u];
                uint32_t frame[2u * VF2_TILE_GLYPH_W * 2u * VF2_TILE_GLYPH_H];
                uint32_t rrow[2u * VF2_TILE_GLYPH_W * VF2_TILE_GLYPH_H];
                uint16_t hi[2] = { 0x8048u, 0x8049u }; /* H, I */
                size_t yy;
                memset(tram, 0, sizeof(tram));
                tram[0] = 0x48u; tram[1] = 0x80u; /* (0,0) = H */
                tram[2] = 0x49u; tram[3] = 0x80u; /* (0,1) = I */
                CHECK(vf2_tile_name_char_at(tram, sizeof(tram), 0u, 0u) == 0x48u);
                CHECK(vf2_tile_name_char_at(tram, sizeof(tram), 0u, 1u) == 0x49u);
                CHECK(vf2_tile_render_layer(tram, sizeof(tram), 2u, 2u,
                                            main_data, size, pal, sizeof(pal),
                                            0u, frame) == VF2_OK);
                /* Row 0 of the frame must equal the per-row render of "HI". */
                CHECK(vf2_tile_render_row_rgb(main_data, size, hi, 2u,
                                              pal, sizeof(pal), 0u, rrow) == VF2_OK);
                for (yy = 0u; yy < VF2_TILE_GLYPH_H; ++yy) {
                    CHECK(memcmp(&frame[yy * (2u * VF2_TILE_GLYPH_W)],
                                 &rrow[yy * (2u * VF2_TILE_GLYPH_W)],
                                 2u * VF2_TILE_GLYPH_W * sizeof(uint32_t)) == 0);
                }
                CHECK(vf2_tile_render_layer(tram, sizeof(tram), 2u, 2u,
                                            main_data, size, pal, sizeof(pal),
                                            1000u, frame) == VF2_ERROR_OUT_OF_BOUNDS);
            }
        }
    }

    free(main_data);
}

/* End-to-end: render the session's tile layer from the cold snapshot (the
 * TEST MENU screen) and require the recovered renderer to draw visible,
 * structured text. */
static void test_session_layer(const char *rom_dir, const char *snapshot)
{
    vf2_session *session = NULL;
    uint32_t *fb = NULL;
    const size_t w = VF2_SESSION_WIDTH;
    const size_t h = VF2_SESSION_HEIGHT;
    size_t nonblack = 0u;
    size_t i;

    if (vf2_session_open(rom_dir, snapshot, &session) != VF2_OK) {
        fprintf(stderr, "skip session layer: cannot open session from %s\n", snapshot);
        return;
    }
    fb = (uint32_t *)calloc(w * h, sizeof(uint32_t));
    if (fb == NULL) { ++failures; vf2_session_close(session); return; }

    CHECK(vf2_session_render_tile_layer(NULL, fb, w, h, 0u) ==
          VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_session_render_tile_layer(session, fb, w, h, 0u) == VF2_OK);

    for (i = 0u; i < w * h; ++i) {
        if ((fb[i] & 0x00ffffffu) != 0u) {
            ++nonblack;
        }
    }
    /* The TEST MENU screen has substantial text; the layer must not be blank. */
    CHECK(nonblack > 1000u);
    fprintf(stderr, "session tile layer: %zu/%zu non-black pixels\n",
            nonblack, w * h);

    free(fb);
    vf2_session_close(session);
}

int main(int argc, char **argv)
{
    test_guards();
    if (argc >= 2) {
        test_font(argv[1]);
    }
    if (argc >= 3) {
        test_session_layer(argv[1], argv[2]);
    }
    if (failures != 0) {
        fprintf(stderr, "%d tile test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("tile tests passed");
    return EXIT_SUCCESS;
}
