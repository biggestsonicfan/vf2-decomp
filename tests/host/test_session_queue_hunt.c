#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/platform.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* Live-filer hunt driver (v0825 T4).
 *
 * Walks oracle frames from a gameplay snapshot with an input
 * pattern and breaks when the CPU lands on a sound-queue writer:
 * the filer (0x43888, captures g0 = the filed command) or the
 * 0x43abc flag-flusher (captures g13 = the record pointer).
 * Queue cells are read back through the hunt accessor for the
 * fixture. Hits are REPORTED evidence, not gated: a clean miss
 * is a measured negative (writer fires on unreached events).
 * Gates: session opens on the real ROMs, audio attaches, the
 * until-walks complete without error, at least one frame runs.
 */

static int g_fail;

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++g_fail; } } while (0)

static void run_hunt(
    const char *rom_dir,
    const char *snapshot,
    uint32_t stop_ip,
    const char *tag,
    const uint32_t *inputs,
    size_t input_count,
    uint32_t budget
)
{
    vf2_session *session = NULL;
    vf2_session_until_report report;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_session_open(rom_dir, snapshot, &session) == VF2_OK);
    if (session == NULL) {
        return;
    }
    CHECK(vf2_session_audio_status(session) == VF2_OK);
    CHECK(vf2_session_step_oracle_until(session, inputs, input_count,
                                       stop_ip, budget,
                                       &report) == VF2_OK);
    fprintf(stderr, "queue-hunt %s: hit=%d frame=%u steps=%llu g0=0x%08x g13=0x%08x ret=0x%08x frames=%u\n",
            tag, report.hit, (unsigned)report.frame,
            (unsigned long long)report.steps, (unsigned)report.g0,
            (unsigned)report.g13, (unsigned)report.ret_addr,
            (unsigned)report.frames_completed);
    CHECK(report.frames_completed > 0u || report.hit != 0);
    if (report.hit != 0) {
        uint8_t qc[3] = {0u, 0u, 0u};
        uint8_t staged[4] = {0u, 0u, 0u, 0u};
        uint8_t bidx = 0u;
        uint8_t ring[4] = {0u, 0u, 0u, 0u};
        if (vf2_session_read_work_ram(session, UINT32_C(0x00504001),
                                      qc, sizeof(qc)) == VF2_OK &&
            vf2_session_read_work_ram(session, UINT32_C(0x00504010),
                                      staged, sizeof(staged)) == VF2_OK &&
            vf2_session_read_work_ram(session, UINT32_C(0x00504014),
                                      &bidx, sizeof(bidx)) == VF2_OK &&
            vf2_session_read_work_ram(session, UINT32_C(0x00504020),
                                      ring, sizeof(ring)) == VF2_OK) {
            fprintf(stderr, "queue-hunt %s: count=%u rcur=%u wcur=%u staged=%02x%02x%02x%02x bidx=%02x ring0=%02x%02x%02x%02x\n",
                    tag, (unsigned)qc[0], (unsigned)qc[1],
                    (unsigned)qc[2], (unsigned)staged[0],
                    (unsigned)staged[1], (unsigned)staged[2],
                    (unsigned)staged[3], (unsigned)bidx,
                    (unsigned)ring[0], (unsigned)ring[1],
                    (unsigned)ring[2], (unsigned)ring[3]);
        }
    }
    vf2_session_close(session);
}

int main(int argc, char **argv)
{
    static const uint32_t pattern[] = {
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
    uint32_t held[2] = {0u, 0u};
    const uint32_t *inputs = pattern;
    size_t input_count = sizeof(pattern) / sizeof(pattern[0]);
    uint32_t budget = 10u;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <rom-dir> <snapshot> [held-input] [alt-input] [frames]\n",
                argv[0]);
        return 2;
    }
    {
        FILE *probe = fopen(argv[2], "rb");
        if (probe == NULL) {
            fprintf(stderr, "queue-hunt: SKIP, no snapshot at %s\n",
                    argv[2]);
            return 77;
        }
        (void)fclose(probe);
    }
    if (argc >= 4) {
        held[0] = (uint32_t)strtoul(argv[3], NULL, 0);
        held[1] = held[0];
        if (argc >= 5) {
            held[1] = (uint32_t)strtoul(argv[4], NULL, 0);
        }
        inputs = held;
        input_count = 2u;
    }
    if (argc >= 6) {
        unsigned long want = strtoul(argv[5], NULL, 0);
        if (want == 0ul || want > 200ul) {
            fprintf(stderr, "queue-hunt: bad frame budget '%s'\n", argv[5]);
            return 2;
        }
        budget = (uint32_t)want;
    }
    run_hunt(argv[1], argv[2], UINT32_C(0x00043888), "filer",
             inputs, input_count, budget);
    run_hunt(argv[1], argv[2], UINT32_C(0x00043abc), "flusher",
             inputs, input_count, budget);
    if (g_fail != 0) {
        fprintf(stderr, "queue-hunt: %d failures\n", g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}
