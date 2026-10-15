/* Polygon-ROM model-record decoder tests (slice 4). Synthetic data only -- no ROM,
 * so this runs in CI. Validates the tgp.c-equivalent strip/quad/skip-3 decode and
 * the object-table w0->w2 lookup, plus fail-closed guards. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/model_decode.h"

static int failures = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);    \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

static void put_u32(uint8_t *p, size_t i, uint32_t v)
{
    p[i * 4u + 0u] = (uint8_t)(v & 0xffu);
    p[i * 4u + 1u] = (uint8_t)((v >> 8) & 0xffu);
    p[i * 4u + 2u] = (uint8_t)((v >> 16) & 0xffu);
    p[i * 4u + 3u] = (uint8_t)((v >> 24) & 0xffu);
}

static uint32_t f2u(float f)
{
    uint32_t u = 0u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

static int close_to(float a, float b)
{
    float d = a - b;
    return d > -1e-5f && d < 1e-5f;
}

static void test_quad_strip_skip3(void)
{
    /* A record: p0=(0,0,0), p1=(1,0,0); attr=0x201 (not end, quad, strip-link 2),
     * skip-3 normal (0,1,0), p2=(1,1,0), p3=(0,1,0); then attr=0 end.
     * Expect 2 triangles: (p0,p1,p2) and (p0,p2,p3). */
    uint8_t rec[80];
    vf2_model_triangle tris[8];
    size_t n = 0u;
    size_t i = 0u;
    memset(rec, 0, sizeof(rec));
    put_u32(rec, 0u, f2u(0.0f)); put_u32(rec, 1u, f2u(0.0f)); put_u32(rec, 2u, f2u(0.0f));
    put_u32(rec, 3u, f2u(1.0f)); put_u32(rec, 4u, f2u(0.0f)); put_u32(rec, 5u, f2u(0.0f));
    put_u32(rec, 6u, 0x00000201u);                       /* attribute: quad, link 2 */
    put_u32(rec, 7u, f2u(0.0f)); put_u32(rec, 8u, f2u(1.0f)); put_u32(rec, 9u, f2u(0.0f)); /* normal skip-3 */
    put_u32(rec, 10u, f2u(1.0f)); put_u32(rec, 11u, f2u(1.0f)); put_u32(rec, 12u, f2u(0.0f)); /* p2 */
    put_u32(rec, 13u, f2u(0.0f)); put_u32(rec, 14u, f2u(1.0f)); put_u32(rec, 15u, f2u(0.0f)); /* p3 */
    put_u32(rec, 16u, 0x00000000u);                      /* end marker */

    CHECK(vf2_model_decode_record(rec, sizeof(rec), 0u, 1, tris, 8u, &n) == VF2_OK);
    CHECK(n == 2u);
    /* tri0 = (0,0,0),(1,0,0),(1,1,0) */
    CHECK(close_to(tris[0].v[0].x, 0.0f) && close_to(tris[0].v[1].x, 1.0f) &&
          close_to(tris[0].v[2].x, 1.0f) && close_to(tris[0].v[2].y, 1.0f));
    /* tri1 = (0,0,0),(1,1,0),(0,1,0) */
    CHECK(close_to(tris[1].v[1].x, 1.0f) && close_to(tris[1].v[1].y, 1.0f) &&
          close_to(tris[1].v[2].x, 0.0f) && close_to(tris[1].v[2].y, 1.0f));
    (void)i;
}

static void test_triangle_no_skip3(void)
{
    /* p0,p1; attr=0x002 (not end, triangle), p2; end. skip3 off. Expect 1 tri. */
    uint8_t rec[48];
    vf2_model_triangle tris[4];
    size_t n = 0u;
    memset(rec, 0, sizeof(rec));
    put_u32(rec, 0u, f2u(2.0f)); put_u32(rec, 1u, f2u(0.0f)); put_u32(rec, 2u, f2u(0.0f));
    put_u32(rec, 3u, f2u(0.0f)); put_u32(rec, 4u, f2u(2.0f)); put_u32(rec, 5u, f2u(0.0f));
    put_u32(rec, 6u, 0x00000002u);                       /* triangle, link 0 */
    put_u32(rec, 7u, f2u(0.0f)); put_u32(rec, 8u, f2u(0.0f)); put_u32(rec, 9u, f2u(2.0f)); /* p2 */
    put_u32(rec, 10u, 0x00000000u);                      /* end */
    CHECK(vf2_model_decode_record(rec, sizeof(rec), 0u, 0, tris, 4u, &n) == VF2_OK);
    CHECK(n == 1u);
    CHECK(close_to(tris[0].v[0].x, 2.0f) && close_to(tris[0].v[1].y, 2.0f) &&
          close_to(tris[0].v[2].z, 2.0f));
}

static void test_guards(void)
{
    uint8_t rec[16];
    vf2_model_triangle tris[2];
    size_t n = 123u;
    memset(rec, 0, sizeof(rec));
    CHECK(vf2_model_decode_record(NULL, 0u, 0u, 1, tris, 2u, &n) ==
          VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_model_decode_record(rec, sizeof(rec), 0u, 1, NULL, 2u, &n) ==
          VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_model_decode_record(rec, sizeof(rec), 2u, 1, tris, 2u, &n) ==
          VF2_ERROR_INVALID_ARGUMENT); /* unaligned */
    /* Too small to hold p0/p1 -> OUT_OF_BOUNDS, count zeroed. */
    CHECK(vf2_model_decode_record(rec, 8u, 0u, 1, tris, 2u, &n) ==
          VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(n == 0u);
}

static void test_lookup(void)
{
    /* Minimal main_data: table at file offset 0x000e0004, one entry at id 3 with
     * w0=0xABCD1234, w2=0x00812345 -> byte=(0x12345)*4=0x48d14, bank=1. */
    static uint8_t md[0x000e0004u + 4u * 16u + 16u];
    uint32_t off = 0u, bank = 0u, id = 0u;
    size_t base = 0x000e0004u + (size_t)3u * 16u;
    memset(md, 0, sizeof(md));
    put_u32(md, base / 4u + 0u, 0xABCD1234u);  /* w0 */
    put_u32(md, base / 4u + 2u, 0x00812345u);  /* w2: bank bit + word index */
    CHECK(vf2_model_lookup_w0(md, sizeof(md), 0xABCD1234u, 0x10u, &off, &bank, &id) == VF2_OK);
    CHECK(off == 0x12345u * 4u);
    CHECK(bank == 1u);
    CHECK(id == 3u);
    CHECK(vf2_model_lookup_w0(md, sizeof(md), 0xDEADBEEFu, 0x10u, &off, &bank, &id) ==
          VF2_ERROR_UNSUPPORTED);
}

int main(void)
{
    test_quad_strip_skip3();
    test_triangle_no_skip3();
    test_guards();
    test_lookup();
    if (failures != 0) {
        fprintf(stderr, "model-decode tests FAILED: %d\n", failures);
        return 1;
    }
    printf("model-decode tests passed\n");
    return 0;
}
