#include <stdio.h>
#include <string.h>

#include "vf2/host.h"
#include "vf2/platform.h"
#include "vf2/session.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++g_fail; } } while (0)

static void test_bindings(void)
{
    vf2_host_bindings b;
    size_t bad = 0u;
    vf2_host_bindings_defaults(&b);
    CHECK(vf2_host_bindings_lookup(&b, "Z") == VF2_PLATFORM_BUTTON_PUNCH);
    CHECK(vf2_host_bindings_lookup(&b, "z") == VF2_PLATFORM_BUTTON_PUNCH);
    CHECK(vf2_host_bindings_lookup(&b, "pad:a") == VF2_PLATFORM_BUTTON_PUNCH);
    CHECK(vf2_host_bindings_lookup(&b, "Nope") == 0u);
    CHECK(vf2_host_button_from_name("p2_kick") == VF2_PLATFORM_BUTTON_P2_KICK);
    CHECK(vf2_host_button_from_name("bogus") == 0u);

    CHECK(vf2_host_bindings_parse(&b,
        "# comment\nSpace = KICK ; trailing\n\n Z = guard\n", &bad) == VF2_OK);
    CHECK(vf2_host_bindings_lookup(&b, "Space") == VF2_PLATFORM_BUTTON_KICK);
    CHECK(vf2_host_bindings_lookup(&b, "Z") == VF2_PLATFORM_BUTTON_GUARD);

    /* Fail closed: a bad line leaves the table untouched (planted gate). */
    {
        vf2_host_bindings before = b;
        CHECK(vf2_host_bindings_parse(&b, "Q = PUNCH\nW = NOTABUTTON\n", &bad)
              != VF2_OK);
        CHECK(bad == 2u);
        CHECK(memcmp(&before, &b, sizeof(b)) == 0);
        CHECK(vf2_host_bindings_parse(&b, "no equals sign\n", &bad) != VF2_OK);
        CHECK(bad == 1u);
    }
}

static void test_input(void)
{
    vf2_host_bindings b;
    vf2_host_input in;
    vf2_host_bindings_defaults(&b);
    vf2_host_input_init(&in, &b);
    CHECK(vf2_host_input_mask(&in) == 0u);
    vf2_host_input_event(&in, "Z", 1);
    CHECK(vf2_host_input_mask(&in) == VF2_PLATFORM_BUTTON_PUNCH);
    /* keyboard and pad both hold PUNCH: releasing one keeps it pressed */
    vf2_host_input_event(&in, "pad:a", 1);
    vf2_host_input_event(&in, "Z", 0);
    CHECK(vf2_host_input_mask(&in) == VF2_PLATFORM_BUTTON_PUNCH);
    vf2_host_input_event(&in, "pad:a", 0);
    CHECK(vf2_host_input_mask(&in) == 0u);
    /* spurious release does not underflow */
    vf2_host_input_event(&in, "Z", 0);
    vf2_host_input_event(&in, "Z", 1);
    CHECK(vf2_host_input_mask(&in) == VF2_PLATFORM_BUTTON_PUNCH);
    vf2_host_input_event(&in, "Left", 1);
    vf2_host_input_event(&in, "Up", 1);
    CHECK(vf2_host_input_mask(&in) == (VF2_PLATFORM_BUTTON_PUNCH |
          VF2_PLATFORM_BUTTON_LEFT | VF2_PLATFORM_BUTTON_UP));
}

static void test_ring(void)
{
    vf2_host_audio_ring r;
    int16_t l[6] = {1, 2, 3, 4, 5, 6};
    int16_t rr[6] = {-1, -2, -3, -4, -5, -6};
    int16_t out[16];

    CHECK(vf2_host_audio_ring_init(&r, 4u) == VF2_OK);
    CHECK(vf2_host_audio_ring_write(&r, l, rr, 6u) == 4u);
    CHECK(r.dropped == 2u);
    vf2_host_audio_ring_read(&r, out, 2u);
    CHECK(out[0] == 1 && out[1] == -1 && out[2] == 2 && out[3] == -2);
    CHECK(vf2_host_audio_ring_write(&r, l + 4, rr + 4, 2u) == 2u);
    vf2_host_audio_ring_read(&r, out, 4u); /* wraps: 3,4,5,6 */
    CHECK(out[0] == 3 && out[2] == 4 && out[4] == 5 && out[6] == 6);
    CHECK(r.underruns == 0u);
    vf2_host_audio_ring_read(&r, out, 3u); /* empty: silence */
    CHECK(r.underruns == 1u);
    CHECK(out[0] == 0 && out[5] == 0);
    vf2_host_audio_ring_destroy(&r);
    CHECK(vf2_host_audio_ring_init(&r, 0u) != VF2_OK);
}

static void test_fit(void)
{
    vf2_host_rect r = vf2_host_fit(1920, 1080, 496, 384, 0);
    CHECK(r.h == 1080 && r.w == 1395 && r.x == (1920 - 1395) / 2 && r.y == 0);
    r = vf2_host_fit(1920, 1080, 496, 384, 1);
    CHECK(r.w == 992 && r.h == 768 && r.x == 464 && r.y == 156);
    r = vf2_host_fit(300, 200, 496, 384, 1); /* smaller than source: scale 1 */
    CHECK(r.w == 496 && r.h == 384);
    r = vf2_host_fit(0, 0, 496, 384, 0);
    CHECK(r.w == 0 && r.h == 0);
}

static void test_session_fail_closed(void)
{
    vf2_session *s = (vf2_session *)(void *)1;
    CHECK(vf2_session_open("/nonexistent-rom-dir", "/nonexistent.vf2snap", &s)
          != VF2_OK);
    CHECK(s == NULL);
    CHECK(vf2_session_open(NULL, "x", &s) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_session_step(NULL, 0u) == VF2_ERROR_INVALID_ARGUMENT);
    {
        vf2_i960_snapshot checkpoint;
        vf2_i960_snapshot_init(&checkpoint);
        CHECK(vf2_session_capture_oracle_snapshot(NULL, &checkpoint) ==
              VF2_ERROR_INVALID_ARGUMENT);
        vf2_i960_snapshot_destroy(&checkpoint);
    }
    CHECK(vf2_session_pixels(NULL) == NULL);
    CHECK(vf2_session_audio_status(NULL) != VF2_OK);
    vf2_session_close(NULL);
}

int main(void)
{
    test_bindings();
    test_input();
    test_ring();
    test_fit();
    test_session_fail_closed();
    if (g_fail != 0) {
        fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    puts("host: ok");
    return 0;
}
