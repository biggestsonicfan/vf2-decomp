#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/platform.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* Oracle-frame UART hunt driver (v0825 8d; window corrected v0853).
 *
 * vf2probe cannot reach sound writes: it injects no frame
 * interrupts, so the game parks in its frame-wait spin (all
 * sixth-fresh runs converge at ip=69536 with identical call
 * counts however inputs are held). This driver steps REAL
 * frames through the interpreter oracle (vf2_session_step_oracle)
 * with a passive bus observer watching the Model 2A sound UART
 * data slot.
 *
 * Window correction (v0853): the slot is 0x01c80000 -- the address
 * the drainer's own `lda` stages (0x43860, disasm-verified) and the
 * address the pump reads back. The 8d driver watched 0x009c0000
 * (the pre-8f table lead), which ROM-staged code never touches, so
 * its zero hits were a measurement of the wrong address.
 *
 * The ROM's only UART writer is the vector-15 sound-drain ISR
 * (0xe10 -> 0x437d0), and the frame loop injects vector 12 only, so
 * an unserviced frame-stepped run still shows zero writes -- that is
 * now a correct measured negative (the oracle never services the
 * sound line). Pass argv[6] = "service" to run
 * vf2_session_service_sound_drain at the end of every frame and see
 * the ROM ISR's stores appear here (write_count counts them; the
 * STOP-ON-WRITE shortcut is disabled in that mode so long plays
 * continue past the initial queue drain).
 *
 * Gates (all ROM-backed, fail closed): session opens on the real
 * ROMs, audio attaches, every frame steps VF2_OK, the work-RAM
 * digest is readable per frame. UART hits are REPORTED evidence,
 * not gated: zero hits is a measured negative in the unserviced
 * mode, nonzero hits are the live-ISR witness.
 */

static int g_fail;

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++g_fail; } } while (0)

/* The sound UART data slot (v0825 8f; the drainer's 0x43860 lda). */
#define UART_BASE UINT32_C(0x01c80000)
#define UART_END UINT32_C(0x01c90000)
#define MAX_HITS 64u

typedef struct uart_hit {
    uint32_t frame;
    uint32_t address;
    uint32_t size;
    uint8_t first_byte;
    uint8_t is_write;
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

/* Watched windows (v0825 table-watch): the UART data range plus the
 * three static 0x009c0000 table entries (debug table @0x111e2, MMIO
 * tables @0x29bd8/@0x50db4, 4 bytes each). ROM data loads go through
 * the observed public bus symbols, so any code that fetches the UART
 * address from its tables shows up here even when it never writes.
 * The tables are kept as the positive control of that read channel
 * (they hold the pre-8f lead's address and are cold by design); the
 * live drain goes straight to UART_BASE via an immediate lda. */
static int in_watched_window(uint32_t address)
{
    if (address >= UART_BASE && address < UART_END) {
        return 1;
    }
    if ((address >= UINT32_C(0x29bd8) && address < UINT32_C(0x29bdc)) ||
        (address >= UINT32_C(0x50db4) && address < UINT32_C(0x50db8)) ||
        (address >= UINT32_C(0x111e2) && address < UINT32_C(0x111e6))) {
        return 1;
    }
    return 0;
}

static void uart_observe(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    uart_capture *capture = (uart_capture *)context;
    int is_uart = 0;

    if (access == NULL || capture == NULL) {
        return;
    }
    if (capture->observed < UINT64_MAX) {
        ++capture->observed;
    }
    if (!in_watched_window(access->address)) {
        return;
    }
    is_uart = (access->address >= UART_BASE && access->address < UART_END);
    if (is_uart && access->kind == VF2_MODEL2A_MEMORY_WRITE &&
        capture->write_count < UINT32_MAX) {
        ++capture->write_count;
    }
    if (capture->hit_count < MAX_HITS && access->data != NULL &&
        access->size != 0u) {
        uart_hit *hit = &capture->hits[capture->hit_count++];
        hit->frame = capture->frame;
        hit->address = access->address;
        hit->size = (uint32_t)access->size;
        hit->first_byte = *(const uint8_t *)access->data;
        hit->is_write =
            (uint8_t)(access->kind == VF2_MODEL2A_MEMORY_WRITE ? 1u : 0u);
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
    int service_sound = 0;
    uint64_t prev_observed = 0u;
    /* Frame budget: default replays the 10-frame evidence pattern
     * once (ctest stays fast); long-play hunts pass more and cycle
     * the pattern. Capped fail-closed. */
    uint32_t budget = (uint32_t)(sizeof(inputs) / sizeof(inputs[0]));
    if (argc >= 7 && strcmp(argv[6], "service") == 0) {
        service_sound = 1;
    }
    if (argc >= 6) {
        unsigned long want = strtoul(argv[5], NULL, 0);
        if (want == 0ul || want > 2000ul) {
            fprintf(stderr, "uart-hunt: bad frame budget '%s'\n", argv[5]);
            return 2;
        }
        budget = (uint32_t)want;
    }

    if (argc < 3) {
        fprintf(stderr,
                "usage: %s <rom-dir> <snapshot> [held-input] [alt-input] "
                "[frames] [service]\n",
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
    for (frame = 0u; frame < budget; ++frame) {
        vf2_status status = VF2_OK;
        uint64_t executed = 0u;
        uint64_t interpreted = 0u;
        uint32_t input =
            inputs[frame % (sizeof(inputs) / sizeof(inputs[0]))];

        /* Optional argv[3] overrides the pattern with one held mask
         * (decimal or 0x-hex): coin runs, start runs, single-move
         * pressure without touching the default evidence. Optional
         * argv[4] is the odd-frame alternate: the driver toggles
         * A,B,A,B... so edge-triggered acceptors (coin slot wants
         * 0->1, not level) see rising edges every other frame. */
        if (argc >= 4) {
            input = (uint32_t)strtoul(argv[3], NULL, 0);
            if (argc >= 5 && (frame & 1u) != 0u) {
                input = (uint32_t)strtoul(argv[4], NULL, 0);
            }
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
        /* Optional live sound-line service (v0853): run the real ROM
         * drain ISR at the frame boundary, the way the enabled device
         * would. Its stores show up in this observer's write_count. */
        if (service_sound) {
            vf2_session_sound_drain_report drain;
            memset(&drain, 0, sizeof(drain));
            if (vf2_session_service_sound_drain(session, &drain) != VF2_OK) {
                fprintf(stderr, "uart-hunt: frame %u sound service failed\n",
                        (unsigned)frame);
                CHECK(0);
                break;
            }
            if (drain.armed != 0u) {
                fprintf(stderr,
                        "uart-hunt: frame %u drain armed=%u ticks=%u bytes=%u popped=%u\n",
                        (unsigned)frame, (unsigned)drain.armed,
                        (unsigned)drain.ticks, (unsigned)drain.bytes_moved,
                        (unsigned)drain.entries_popped);
            }
        }
        /* Positive control: the oracle retires thousands of bus
         * accesses per frame through the observed symbols. A frame
         * with none means the hook is dead, not that the game is
         * silent -- fail rather than report a void negative. */
        CHECK(capture.observed > prev_observed);
        prev_observed = capture.observed;
        /* Long-play hunts stop at the first UART write: the byte +
         * frame stamp below is the bridge fixture. In service mode the
         * writes are the expected ISR drain, so runs continue. */
        if (capture.write_count > 0u && !service_sound) {
            fprintf(stderr, "uart-hunt: STOP-ON-WRITE at frame %u\n",
                    (unsigned)frame);
            ++frame;
            break;
        }
        vf2_session_instruction_counts(session, &executed, &interpreted);
        fprintf(stderr, "uart-hunt: frame %u digest=0x%08x exec=%llu interp=%llu obs=%llu writes=%u input=0x%x\n",
                (unsigned)frame, (unsigned)digest,
                (unsigned long long)executed,
                (unsigned long long)interpreted,
                (unsigned long long)capture.observed,
                (unsigned)capture.write_count,
                (unsigned)input);
    }
    /* Detach: leave no instrumentation behind. */
    CHECK(vf2_session_set_memory_observer(session, NULL, NULL) == VF2_OK);
    CHECK(vf2_session_set_memory_observer(NULL, uart_observe, &capture) !=
          VF2_OK);
    fprintf(stderr, "uart-hunt: %u UART writes in %u frames\n",
            (unsigned)capture.write_count, (unsigned)frame);
    for (hit = 0u; hit < capture.hit_count; ++hit) {
        fprintf(stderr, "uart-hunt: hit f=%u %s addr=0x%08x size=%u byte=0x%02x\n",
                (unsigned)capture.hits[hit].frame,
                capture.hits[hit].is_write ? "W" : "R",
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
