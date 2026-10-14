#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/platform.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* Oracle-frame UART hunt driver (v0825 8d).
 *
 * vf2probe cannot reach sound writes: it injects no frame
 * interrupts, so the game parks in its frame-wait spin (all
 * sixth-fresh runs converge at ip=69536 with identical call
 * counts however inputs are held). This driver steps REAL
 * frames through the interpreter oracle (vf2_session_step_oracle)
 * with a passive bus observer watching the Model 2A serial port
 * (0x009c0000, main->sound UART), the architected sound-command
 * channel.
 *
 * Gates (all ROM-backed, fail closed): session opens on the real
 * ROMs, audio attaches, every frame steps VF2_OK, the work-RAM
 * digest is readable per frame. UART hits are REPORTED evidence,
 * not gated: zero hits is a measured negative (writer fires on
 * unreached events), nonzero hits are the bridge fixture.
 */

static int g_fail;

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++g_fail; } } while (0)

#define UART_BASE UINT32_C(0x009c0000)
#define UART_END UINT32_C(0x009d0000)
#define MAX_HITS 64u

typedef struct uart_hit {
    uint32_t frame;
    uint32_t address;
    uint32_t size;
    uint8_t first_byte;
} uart_hit;

typedef struct uart_capture {
    uart_hit hits[MAX_HITS];
    uint32_t hit_count;
    uint32_t write_count;
    uint32_t frame;
    /* Every observed access, UART or not: the per-frame print of this
     * is the hook's positive control. exec > 0 with observed == 0
     * means the observer is dead and the run's negative is void. */
    uint64_t observed;
} uart_capture;

static void uart_observe(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    uart_capture *capture = (uart_capture *)context;

    if (access == NULL || capture == NULL) {
        return;
    }
    if (capture->observed < UINT64_MAX) {
        ++capture->observed;
    }
    if (access->kind != VF2_MODEL2A_MEMORY_WRITE ||
        access->address < UART_BASE || access->address >= UART_END) {
        return;
    }
    if (capture->write_count < UINT32_MAX) {
        ++capture->write_count;
    }
    if (capture->hit_count < MAX_HITS && access->data != NULL &&
        access->size != 0u) {
        uart_hit *hit = &capture->hits[capture->hit_count++];
        hit->frame = capture->frame;
        hit->address = access->address;
        hit->size = (uint32_t)access->size;
        hit->first_byte = *(const uint8_t *)access->data;
    }
}

int main(int argc, char **argv)
{
    static const uint32_t inputs[] = {
        VF2_PLATFORM_BUTTON_PUNCH,
        VF2_PLATFORM_BUTTON_PUNCH,
        VF2_PLATFORM_BUTTON_PUNCH,
        VF2_PLATFORM_BUTTON_PUNCH,
        VF2_PLATFORM_BUTTON_PUNCH | VF2_PLATFORM_BUTTON_KICK,
        VF2_PLATFORM_BUTTON_START,
        VF2_PLATFORM_BUTTON_PUNCH,
        VF2_PLATFORM_BUTTON_KICK,
        VF2_PLATFORM_BUTTON_PUNCH,
        VF2_PLATFORM_BUTTON_PUNCH,
    };
    static uart_capture capture;
    vf2_session *session = NULL;
    uint32_t digest = 0u;
    uint32_t frame = 0u;
    uint32_t hit = 0u;
    uint64_t prev_observed = 0u;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <rom-dir> <snapshot> [held-input]\n",
                argv[0]);
        return 2;
    }
    /* Skip where the gitignored snapshot corpus is absent (p1-demo
     * precedent): ctest maps this to Not Run, suite stays green. */
    {
        FILE *probe = fopen(argv[2], "rb");
        if (probe == NULL) {
            fprintf(stderr, "uart-hunt: SKIP, no snapshot at %s\n", argv[2]);
            return 77;
        }
        (void)fclose(probe);
    }
    memset(&capture, 0, sizeof(capture));
    CHECK(vf2_session_open(argv[1], argv[2], &session) == VF2_OK);
    if (session == NULL) {
        return 1;
    }
    fprintf(stderr, "uart-hunt: audio_status=%d\n",
            (int)vf2_session_audio_status(session));
    CHECK(vf2_session_audio_status(session) == VF2_OK);
    CHECK(vf2_session_set_memory_observer(
              session, uart_observe, &capture) == VF2_OK);
    for (frame = 0u;
         frame < sizeof(inputs) / sizeof(inputs[0]);
         ++frame) {
        vf2_status status = VF2_OK;
        uint64_t executed = 0u;
        uint64_t interpreted = 0u;
        uint32_t input = inputs[frame];

        /* Optional argv[3] overrides the pattern with one held mask
         * (decimal or 0x-hex): coin runs, start runs, single-move
         * pressure without touching the default evidence. */
        if (argc >= 4) {
            input = (uint32_t)strtoul(argv[3], NULL, 0);
        }

        /* Stamp the current frame for hits recorded during it. */
        capture.frame = frame;
        status = vf2_session_step_oracle(session, input);
        if (status != VF2_OK) {
            fprintf(stderr, "uart-hunt: frame %u step failed: %d\n",
                    (unsigned)frame, (int)status);
            CHECK(0);
            break;
        }
        CHECK(vf2_session_work_ram_digest(session, &digest) == VF2_OK);
        /* Positive control: the oracle retires thousands of bus
         * accesses per frame through the observed symbols. A frame
         * with none means the hook is dead, not that the game is
         * silent -- fail rather than report a void negative. */
        CHECK(capture.observed > prev_observed);
        prev_observed = capture.observed;
        vf2_session_instruction_counts(session, &executed, &interpreted);
        fprintf(stderr, "uart-hunt: frame %u digest=0x%08x exec=%llu interp=%llu obs=%llu writes=%u\n",
                (unsigned)frame, (unsigned)digest,
                (unsigned long long)executed,
                (unsigned long long)interpreted,
                (unsigned long long)capture.observed,
                (unsigned)capture.write_count);
    }
    /* Detach: leave no instrumentation behind. */
    CHECK(vf2_session_set_memory_observer(session, NULL, NULL) == VF2_OK);
    CHECK(vf2_session_set_memory_observer(NULL, uart_observe, &capture) !=
          VF2_OK);
    fprintf(stderr, "uart-hunt: %u UART writes in %u frames\n",
            (unsigned)capture.write_count, (unsigned)frame);
    for (hit = 0u; hit < capture.hit_count; ++hit) {
        fprintf(stderr, "uart-hunt: hit f=%u addr=0x%08x size=%u byte=0x%02x\n",
                (unsigned)capture.hits[hit].frame,
                (unsigned)capture.hits[hit].address,
                (unsigned)capture.hits[hit].size,
                (unsigned)capture.hits[hit].first_byte);
    }
    vf2_session_close(session);
    if (g_fail != 0) {
        fprintf(stderr, "uart-hunt: %d failures\n", g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}
