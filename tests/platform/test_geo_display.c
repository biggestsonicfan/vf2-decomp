/* Geometriser display-list walker tests. Synthetic buffers only -- no RAM9 dump,
 * no capture, so this runs in CI. Validates exact opcode sizing (fixed, count-
 * based, the variable direct-data and test commands), jump following, matrix /
 * translate tracking, object extraction, the out[] limit and bounds safety, and
 * the buffer builder's geo-function fold + the prg microcode-upload gate. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/geo_display.h"

static int failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);  \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static uint32_t f2u(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

/* Build an opcode word: command in bits 23-27 (the walker ignores the rest). */
static uint32_t op(uint32_t cmd)
{
    return (cmd & 0x1fu) << 23;
}

/* Exact sizing + matrix/translate tracking + object extraction. */
static void test_sequence(void)
{
    uint32_t buf[64];
    vf2_geo_object out[8];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;
    size_t w = 0u;
    size_t i;

    buf[w++] = op(0x0bu); /* matrix write: 12 floats */
    for (i = 0u; i < 12u; ++i) {
        buf[w++] = f2u((float)(i + 1)); /* mat[i] = i+1; translate = 10,11,12 */
    }
    buf[w++] = op(0x01u); /* object */
    buf[w++] = 0x111u;
    buf[w++] = 0x222u;
    buf[w++] = 0x333u;
    buf[w++] = 0x444u;
    buf[w++] = op(0x0cu); /* translate: mat[9..11] = 100,200,300 */
    buf[w++] = f2u(100.0f);
    buf[w++] = f2u(200.0f);
    buf[w++] = f2u(300.0f);
    buf[w++] = op(0x11u); /* object (alias opcode 0x11) */
    buf[w++] = 0x555u;
    buf[w++] = 0x666u;
    buf[w++] = 0x777u;
    buf[w++] = 0x888u;
    buf[w++] = op(0x0fu); /* end */

    CHECK(vf2_geo_walk(buf, w, 0u, out, 8u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 2u);
    CHECK(out[0].tpa == 0x111u && out[0].tha == 0x222u && out[0].oba == 0x333u &&
          out[0].obc == 0x444u);
    CHECK(out[0].matrix[0] == 1.0f);
    CHECK(out[0].matrix[9] == 10.0f && out[0].matrix[11] == 12.0f);
    CHECK(out[1].tpa == 0x555u);
    /* translate updated only the last three; the 3x3 stays */
    CHECK(out[1].matrix[0] == 1.0f);
    CHECK(out[1].matrix[9] == 100.0f && out[1].matrix[10] == 200.0f &&
          out[1].matrix[11] == 300.0f);
}

/* A jump opcode must continue at its target and skip the words before it. */
static void test_jump(void)
{
    uint32_t buf[32];
    vf2_geo_object out[4];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;

    memset(buf, 0, sizeof(buf));
    /* word 0: jump to byte offset of word 10 */
    buf[0] = 0x80000000u | (10u * 4u);
    /* decoy object words in between (must NOT be parsed) */
    buf[1] = op(0x01u);
    buf[2] = 0xdeadu;
    /* target at word 10 */
    buf[10] = op(0x01u);
    buf[11] = 0xabcu;
    buf[12] = 0xdefu;
    buf[13] = 0x123u;
    buf[14] = 0x456u;
    buf[15] = op(0x0fu);

    CHECK(vf2_geo_walk(buf, 32u, 0u, out, 4u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 1u);
    CHECK(out[0].tpa == 0xabcu);
}

/* Count-based command (polygon data = 2 + count) must be sized exactly so the
 * following object lands where expected. */
static void test_count_sized(void)
{
    uint32_t buf[32];
    vf2_geo_object out[4];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;
    size_t w = 0u;

    buf[w++] = op(0x05u); /* polygon data */
    buf[w++] = 0x01000000u; /* address */
    buf[w++] = 3u;          /* count */
    buf[w++] = 0xa; buf[w++] = 0xb; buf[w++] = 0xc; /* 3 data words */
    buf[w++] = op(0x01u);
    buf[w++] = 0x999u; buf[w++] = 0u; buf[w++] = 0u; buf[w++] = 0u;
    buf[w++] = op(0x0fu);

    CHECK(vf2_geo_walk(buf, w, 0u, out, 4u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 1u && out[0].tpa == 0x999u);
}

/* Variable direct-data (opcode 0x02): 8 base words + per-attr blocks (quad adds
 * 3), terminated by an attr with (attr & 3) == 0. */
static void test_direct_data(void)
{
    uint32_t buf[48];
    vf2_geo_object out[4];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;
    size_t w = 0u;
    size_t i;

    buf[w++] = op(0x02u);
    buf[w++] = 0u; buf[w++] = 0u;          /* tpa, tha */
    for (i = 0u; i < 6u; ++i) buf[w++] = 0u; /* two xyz points */
    buf[w++] = 0x00000003u;                 /* attr: quad (bit0), (&3)!=0 */
    for (i = 0u; i < 8u; ++i) buf[w++] = 0u; /* luma,dist,xyz + quad xyz */
    buf[w++] = 0x00000002u;                 /* attr: tri, (&3)!=0 */
    for (i = 0u; i < 5u; ++i) buf[w++] = 0u; /* luma,dist,xyz */
    buf[w++] = 0x00000000u;                 /* terminating attr */
    buf[w++] = op(0x01u);                   /* object right after */
    buf[w++] = 0x7a7au; buf[w++] = 0u; buf[w++] = 0u; buf[w++] = 0u;
    buf[w++] = op(0x0fu);

    CHECK(vf2_geo_walk(buf, w, 0u, out, 4u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 1u && out[0].tpa == 0x7a7au);
}

/* Geo test (opcode 0x0e): 32 fifo words + 1 block-count + blocks*3. */
static void test_geo_test_opcode(void)
{
    uint32_t buf[64];
    vf2_geo_object out[4];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;
    size_t w = 0u;
    size_t i;

    buf[w++] = op(0x0eu);
    for (i = 0u; i < 32u; ++i) buf[w++] = 0u; /* fifo test words */
    buf[w++] = 1u;                             /* one block */
    buf[w++] = 0u; buf[w++] = 0u; buf[w++] = 0u; /* address,count,checksum */
    buf[w++] = op(0x01u);
    buf[w++] = 0x5e5eu; buf[w++] = 0u; buf[w++] = 0u; buf[w++] = 0u;
    buf[w++] = op(0x0fu);

    CHECK(vf2_geo_walk(buf, w, 0u, out, 4u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 1u && out[0].tpa == 0x5e5eu);
}

/* out[] limit -> VF2_GEO_STOP_FULL. */
static void test_full(void)
{
    uint32_t buf[32];
    vf2_geo_object out[2];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_END;
    size_t w = 0u;
    int k;

    for (k = 0; k < 3; ++k) {
        buf[w++] = op(0x01u);
        buf[w++] = (uint32_t)(0x10 + k);
        buf[w++] = 0u; buf[w++] = 0u; buf[w++] = 0u;
    }
    buf[w++] = op(0x0fu);

    CHECK(vf2_geo_walk(buf, w, 0u, out, 2u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_FULL);
    CHECK(n == 2u);
}

/* A command whose operands run past the end stops with BOUNDS, no OOB read. */
static void test_bounds(void)
{
    uint32_t buf[4];
    vf2_geo_object out[2];
    size_t n = 7u;
    vf2_geo_stop stop = VF2_GEO_STOP_END;

    buf[0] = op(0x0bu); /* matrix needs 12 words; only 3 follow */
    buf[1] = 0u; buf[2] = 0u; buf[3] = 0u;
    CHECK(vf2_geo_walk(buf, 4u, 0u, out, 2u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_BOUNDS);
    CHECK(n == 0u);

    CHECK(vf2_geo_walk(NULL, 4u, 0u, out, 2u, &n, &stop, NULL) ==
          VF2_ERROR_INVALID_ARGUMENT);
}

/* Builder: the prg gate drops microcode-upload words; geo-function writes fold
 * the function into the pushed opcode; the built list parses back. */
static void test_builder_prg_gate(void)
{
    uint32_t buf[16];
    vf2_geo_builder b;
    vf2_geo_object out[4];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;

    memset(buf, 0, sizeof(buf));
    vf2_geo_builder_init(&b, buf, 16u);

    /* microcode-upload mode: these prg words must be dropped, not buffered */
    vf2_geo_builder_set_geoctl(&b, 0x80000000u);
    vf2_geo_builder_prg_write(&b, 0xdead0001u);
    vf2_geo_builder_prg_write(&b, 0xdead0002u);
    CHECK(b.write_ptr == 0u); /* gate did not advance the write pointer */

    /* normal mode: build object {0x111,0x222,0x333,0x444} then end */
    vf2_geo_builder_set_geoctl(&b, 0u);
    vf2_geo_builder_geo_write(&b, 0x1008u, 0u);  /* write pointer = 0 */
    vf2_geo_builder_geo_write(&b, 0x3008u, 0u);  /* read pointer = 0 */
    vf2_geo_builder_geo_write(&b, 0x0010u, 0u);  /* function 1 -> object opcode */
    vf2_geo_builder_prg_write(&b, 0x111u);
    vf2_geo_builder_prg_write(&b, 0x222u);
    vf2_geo_builder_prg_write(&b, 0x333u);
    vf2_geo_builder_prg_write(&b, 0x444u);
    vf2_geo_builder_geo_write(&b, 0x00f0u, 0u); /* function 0x0f -> end opcode */

    /* the dropped microcode words are nowhere in the buffer */
    CHECK(buf[0] != 0xdead0001u);

    CHECK(vf2_geo_walk(buf, 16u, b.read_ptr, out, 4u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 1u);
    CHECK(out[0].tpa == 0x111u && out[0].tha == 0x222u &&
          out[0].oba == 0x333u && out[0].obc == 0x444u);
}

/* Builder: read/write pointers at a non-zero offset still resolve. */
static void test_builder_offset(void)
{
    uint32_t buf[32];
    vf2_geo_builder b;
    vf2_geo_object out[4];
    size_t n = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;

    memset(buf, 0, sizeof(buf));
    vf2_geo_builder_init(&b, buf, 32u);
    vf2_geo_builder_geo_write(&b, 0x1008u, 40u); /* write pointer = byte 40 (word 10) */
    vf2_geo_builder_geo_write(&b, 0x3008u, 40u); /* read from the same place */
    vf2_geo_builder_geo_write(&b, 0x0010u, 0u);  /* object opcode */
    vf2_geo_builder_prg_write(&b, 0xc0feu);
    vf2_geo_builder_prg_write(&b, 0u);
    vf2_geo_builder_prg_write(&b, 0u);
    vf2_geo_builder_prg_write(&b, 0u);
    vf2_geo_builder_geo_write(&b, 0x00f0u, 0u);  /* end */

    CHECK(b.read_ptr == 40u);
    CHECK(vf2_geo_walk(buf, 32u, b.read_ptr, out, 4u, &n, &stop, NULL) == VF2_OK);
    CHECK(stop == VF2_GEO_STOP_END);
    CHECK(n == 1u && out[0].tpa == 0xc0feu);
}

int main(void)
{
    test_sequence();
    test_jump();
    test_count_sized();
    test_direct_data();
    test_geo_test_opcode();
    test_full();
    test_bounds();
    test_builder_prg_gate();
    test_builder_offset();

    if (failures == 0) {
        printf("test_geo_display: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "test_geo_display: %d checks failed\n", failures);
    return 1;
}
