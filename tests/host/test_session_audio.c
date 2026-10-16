#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/host.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* Session->host audio plumbing (v0825 8e).
 *
 * Proves the session render path and the host ring handoff are
 * wired and stable, without staging packets the session never
 * produced (producer still unmapped -- see the 8c hunt note).
 * Silence is EXPECTED and reported, not gated: no in-session
 * producer keys voices, so the freshly attached board renders
 * zeros. What IS gated: attach OK, renders OK, bit-identical
 * re-renders (determinism), lossless ring round-trip, and a
 * correctly-sized WAV artifact in out/ (gitignored).
 */

static int g_fail;

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++g_fail; } } while (0)

#define FRAMES 735u

static void write_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
    out[2] = (uint8_t)(value >> 16u);
    out[3] = (uint8_t)(value >> 24u);
}

static void write_u16_le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
}

int main(int argc, char **argv)
{
    static int16_t left_a[FRAMES];
    static int16_t right_a[FRAMES];
    static int16_t left_b[FRAMES];
    static int16_t right_b[FRAMES];
    static int16_t interleaved[FRAMES * 2u * 2u];
    static vf2_host_audio_ring ring;
    vf2_session *session = NULL;
    size_t i = 0u;
    int32_t peak = 0;
    FILE *wav = NULL;
    uint8_t header[44];

    if (argc < 3) {
        fprintf(stderr, "usage: %s <rom-dir> <snapshot>\n", argv[0]);
        return 2;
    }
    {
        FILE *probe = fopen(argv[2], "rb");
        if (probe == NULL) {
            fprintf(stderr, "session-audio: SKIP, no snapshot at %s\n",
                    argv[2]);
            return 77;
        }
        (void)fclose(probe);
    }
    CHECK(vf2_session_open(argv[1], argv[2], &session) == VF2_OK);
    if (session == NULL) {
        return 1;
    }
    CHECK(vf2_session_audio_status(session) == VF2_OK);
    CHECK(vf2_session_render_audio(session, left_a, right_a, FRAMES) ==
          VF2_OK);
    CHECK(vf2_session_render_audio(session, left_b, right_b, FRAMES) ==
          VF2_OK);
    CHECK(memcmp(left_a, left_b, sizeof(left_a)) == 0);
    CHECK(memcmp(right_a, right_b, sizeof(right_b)) == 0);
    for (i = 0u; i < FRAMES; ++i) {
        int32_t al = left_a[i] < 0 ? -(int32_t)left_a[i] : left_a[i];
        int32_t ar = right_a[i] < 0 ? -(int32_t)right_a[i] : right_a[i];
        if (al > peak) {
            peak = al;
        }
        if (ar > peak) {
            peak = ar;
        }
    }
    fprintf(stderr, "session-audio: peak=%d (0 expected, no producer)\n",
            (int)peak);
    /* Host ring round-trip: the exact push/read the SDL shell does
     * per frame, verified lossless here. */
    CHECK(vf2_host_audio_ring_init(&ring, FRAMES * 2u) == VF2_OK);
    CHECK(vf2_host_audio_ring_write(&ring, left_a, right_a, FRAMES) ==
          FRAMES);
    CHECK(vf2_host_audio_ring_write(&ring, left_b, right_b, FRAMES) ==
          FRAMES);
    memset(interleaved, 0x5au, sizeof(interleaved));
    vf2_host_audio_ring_read(&ring, interleaved, FRAMES * 2u);
    for (i = 0u; i < FRAMES; ++i) {
        CHECK(interleaved[i * 2u] == left_a[i]);
        CHECK(interleaved[i * 2u + 1u] == right_a[i]);
        CHECK(interleaved[(FRAMES + i) * 2u] == left_b[i]);
        CHECK(interleaved[(FRAMES + i) * 2u + 1u] == right_b[i]);
    }
    vf2_host_audio_ring_destroy(&ring);
    /* WAV artifact (out/, gitignored): sizes computed, not copied. */
    memcpy(header, "RIFF....WAVEfmt ", 16);
    write_u32_le(header + 4u, 36u + FRAMES * 4u);
    write_u32_le(header + 16u, 16u);
    write_u16_le(header + 20u, 1u);
    write_u16_le(header + 22u, 2u);
    write_u32_le(header + 24u, VF2_SESSION_SAMPLE_RATE);
    write_u32_le(header + 28u, VF2_SESSION_SAMPLE_RATE * 4u);
    write_u16_le(header + 32u, 4u);
    write_u16_le(header + 34u, 16u);
    memcpy(header + 36u, "data", 4);
    write_u32_le(header + 40u, FRAMES * 4u);
    wav = fopen("out/session-audio.wav", "wb");
    if (wav != NULL) {
        uint8_t frame[4];
        CHECK(fwrite(header, 1u, sizeof(header), wav) == sizeof(header));
        for (i = 0u; i < FRAMES; ++i) {
            frame[0] = (uint8_t)left_a[i];
            frame[1] = (uint8_t)((uint16_t)left_a[i] >> 8u);
            frame[2] = (uint8_t)right_a[i];
            frame[3] = (uint8_t)((uint16_t)right_a[i] >> 8u);
            CHECK(fwrite(frame, 1u, sizeof(frame), wav) == sizeof(frame));
        }
        (void)fclose(wav);
        fprintf(stderr, "session-audio: wrote out/session-audio.wav\n");
    } else {
        fprintf(stderr, "session-audio: out/ missing, WAV skipped\n");
    }
    vf2_session_close(session);
    /* Optional pump leg (T3): open a second session from a
     * live-queue snapshot (park-sixth-a6c0 holds 3 filed entries)
     * and bridge them into audible-path state. Gates: 9 bytes
     * moved over 9 drain ticks, 3 entries filed, then game_update
     * (L1/L2 ticks) + render determinism. The 90-class entry
     * misses on the init-state board (no staged voice, $152e
     * door -- the admitted 90 shape is proven with S4 arming in
     * the T2 differential, never staged here), both a0 entries
     * refuse by design, so 0 dispatched / 3 refused. Peak is
     * reported, not gated (no engine keying in-session). */
    if (argc >= 4) {
        static int16_t pleft_a[FRAMES];
        static int16_t pright_a[FRAMES];
        static int16_t pleft_b[FRAMES];
        static int16_t pright_b[FRAMES];
        vf2_session *pump = NULL;
        vf2_session_audio_pump_report preport;
        size_t p = 0u;
        int32_t ppeak = 0;
        FILE *probe = fopen(argv[3], "rb");
        if (probe == NULL) {
            fprintf(stderr, "session-audio: SKIP pump, no snapshot at %s\n",
                    argv[3]);
            return 77;
        }
        (void)fclose(probe);
        memset(&preport, 0, sizeof(preport));
        CHECK(vf2_session_open(argv[1], argv[3], &pump) == VF2_OK);
        if (pump == NULL) {
            return 1;
        }
        CHECK(vf2_session_audio_status(pump) == VF2_OK);
        CHECK(vf2_session_pump_audio(pump, &preport) == VF2_OK);
        fprintf(stderr, "session-audio: pump bytes=%u filed=%u dispatched=%u refused=%u ticks=%u drain=%d\n",
                (unsigned)preport.bytes_moved,
                (unsigned)preport.entries_filed,
                (unsigned)preport.entries_dispatched,
                (unsigned)preport.entries_refused,
                (unsigned)preport.drain_ticks,
                (int)preport.drain_status);
        CHECK(preport.drain_status == VF2_OK);
        CHECK(preport.bytes_moved == 9u);
        CHECK(preport.drain_ticks == 9u);
        CHECK(preport.entries_filed == 3u);
        /* Init-state board: the 90-class entry misses (no staged
         * voice, $152e door) and both a0 entries refuse by design,
         * so 0 dispatched / 3 refused. The admitted 90 shape is
         * proven with S4 arming in the T2 differential, not here:
         * the pump never stages voices. */
        CHECK(preport.entries_dispatched == 0u);
        CHECK(preport.entries_refused == 3u);
        CHECK(vf2_session_render_audio(pump, pleft_a, pright_a,
                                      FRAMES) == VF2_OK);
        CHECK(vf2_session_render_audio(pump, pleft_b, pright_b,
                                      FRAMES) == VF2_OK);
        CHECK(memcmp(pleft_a, pleft_b, sizeof(pleft_a)) == 0);
        CHECK(memcmp(pright_a, pright_b, sizeof(pright_a)) == 0);
        for (p = 0u; p < FRAMES; ++p) {
            int32_t al = pleft_a[p] < 0 ? -(int32_t)pleft_a[p] : pleft_a[p];
            int32_t ar = pright_a[p] < 0 ? -(int32_t)pright_a[p] : pright_a[p];
            if (al > ppeak) {
                ppeak = al;
            }
            if (ar > ppeak) {
                ppeak = ar;
            }
        }
        fprintf(stderr, "session-audio: pump peak=%d\n", (int)ppeak);
        vf2_session_close(pump);
    }
    if (g_fail != 0) {
        fprintf(stderr, "session-audio: %d failures\n", g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}
