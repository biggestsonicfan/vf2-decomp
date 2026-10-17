#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* Sound-drain interrupt (vector 15) live-service fixture (v0853).
 *
 * The ROM arms the sound line itself (filer 0x43888 pokes [0xe80004]
 * 0x21 then 0x421 on accept) and drains it through the vector-15
 * ISR: handler 0xe10 -> call 0x437d0, one byte per service to the
 * UART data slot 0x01c80000, re-arming while entries remain, ending
 * disarmed through the ROM's own empty pass. The oracle never runs
 * this ISR (the frame loop injects vector 12 only) -- the 8c/8f hunts
 * therefore measured a void negative (and watched 0x009c0000, not
 * the ROM-staged 0x01c80000). This fixture drives
 * vf2_session_service_sound_drain on the live park queue and pins:
 *
 *   leg 1 park state: real ISR -> 9 bytes ae 10 1f 9e 0a 7f ae 10 1f,
 *     10 ticks, 3 entries popped, queue end cells, and the exact bus
 *     behavior (9 UART stores, 38 enable stores = 20x 0x21 + 18x
 *     0x421, 10 request acks, 289 instructions);
 *   leg 2 pump equivalence: the shadow-CPU C drainer emits the same
 *     bytes and queue cells; only the enable end differs (0x421 --
 *     it stops one tick before the ROM's disarm pass);
 *   leg 3 convergence: draining at the parked state vs one frame
 *     later at the vector-12 boundary converges to identical
 *     per-frame work-RAM digests;
 *   leg 4 fail-closed: a native-runtime-stepped session refuses the
 *     service untouched (and an unarmed line reports zero work).
 *
 * Needs the gitignored park + sixth-fresh snapshots; skips (exit 77)
 * where they are absent, per the p1-demo precedent. */

static int g_fail;

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++g_fail; } } while (0)

enum {
    QUEUE_COUNT = 0x00504001,
    QUEUE_RCUR = 0x00504002,
    QUEUE_WCUR = 0x00504003,
    QUEUE_STAGED = 0x00504010,
    QUEUE_BYTE_INDEX = 0x00504014,
    UART_DATA = 0x01c80000,
    SOUND_ENABLE = 0x00e80004,
    SOUND_REQUEST = 0x00e80000,
    CAPTURE_CAP = 64
};

static const uint8_t g_expected_bytes[9] = {
    0xae, 0x10, 0x1f, 0x9e, 0x0a, 0x7f, 0xae, 0x10, 0x1f
};

typedef struct bus_capture {
    uint8_t uart[CAPTURE_CAP];
    uint32_t uart_count;
    uint32_t enable_count;   /* stores to [0xe80004] */
    uint32_t enable_disables;/* ... with value 0x21 */
    uint32_t enable_arms;    /* ... with value 0x421 */
    uint32_t last_enable;
    uint32_t request_count;  /* stores to [0xe80000] */
    uint32_t request_acks;   /* ... with the 0xfffffbff ack mask */
    uint64_t observed;
} bus_capture;

static uint32_t load_le(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t value = 0u;
    size_t index = 0u;

    for (index = 0u; index < size && index < 4u; ++index) {
        value |= (uint32_t)bytes[index] << (8u * index);
    }
    return value;
}

static void bus_observe(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    bus_capture *capture = (bus_capture *)context;
    uint32_t value = 0u;

    if (access == NULL || capture == NULL) {
        return;
    }
    if (capture->observed < UINT64_MAX) {
        ++capture->observed;
    }
    if (access->kind != VF2_MODEL2A_MEMORY_WRITE || access->data == NULL ||
        access->size == 0u) {
        return;
    }
    value = load_le(access->data, access->size);
    if (access->address == UART_DATA) {
        if (capture->uart_count < CAPTURE_CAP) {
            capture->uart[capture->uart_count++] = (uint8_t)value;
        }
    } else if (access->address == SOUND_ENABLE) {
        ++capture->enable_count;
        capture->last_enable = value;
        if (value == UINT32_C(0x21)) {
            ++capture->enable_disables;
        } else if (value == UINT32_C(0x421)) {
            ++capture->enable_arms;
        }
    } else if (access->address == SOUND_REQUEST) {
        ++capture->request_count;
        if (value == UINT32_C(0xfffffbff)) {
            ++capture->request_acks;
        }
    }
}

static uint8_t read_u8(vf2_session *session, uint32_t address)
{
    uint8_t value = 0u;
    CHECK(vf2_session_read_work_ram(session, address, &value, 1u) == VF2_OK);
    return value;
}

static uint32_t read_u32(vf2_session *session, uint32_t address)
{
    uint8_t bytes[4];
    memset(bytes, 0, sizeof(bytes));
    CHECK(vf2_session_read_work_ram(session, address, bytes, sizeof(bytes)) ==
          VF2_OK);
    return load_le(bytes, sizeof(bytes));
}

/* Leg 1 + the unarmed control: the real ISR drains the live park queue. */
static void leg_live_service(const char *rom, const char *snap)
{
    static bus_capture capture;
    vf2_session *session = NULL;
    vf2_session_sound_drain_report drain;
    vf2_session_sound_drain_report again;
    uint64_t executed = 0u, interpreted = 0u;
    uint64_t executed_after = 0u, interpreted_after = 0u;
    uint64_t executed_end = 0u, interpreted_end = 0u;

    memset(&capture, 0, sizeof(capture));
    memset(&drain, 0, sizeof(drain));
    CHECK(vf2_session_open(rom, snap, &session) == VF2_OK);
    if (session == NULL) {
        return;
    }
    CHECK(vf2_session_set_memory_observer(session, bus_observe, &capture) ==
          VF2_OK);
    vf2_session_instruction_counts(session, &executed, &interpreted);
    CHECK(vf2_session_service_sound_drain(session, &drain) == VF2_OK);
    vf2_session_instruction_counts(session, &executed_after,
                                   &interpreted_after);

    CHECK(drain.armed == 1u);
    CHECK(drain.ticks == 10u);
    CHECK(drain.bytes_moved == 9u);
    CHECK(drain.entries_popped == 3u);
    CHECK(memcmp(drain.bytes, g_expected_bytes, sizeof(g_expected_bytes)) == 0);

    /* The bus-level behavior the ROM actually produced. */
    CHECK(capture.uart_count == 9u);
    CHECK(memcmp(capture.uart, g_expected_bytes, sizeof(g_expected_bytes)) == 0);
    CHECK(capture.enable_count == 38u);
    CHECK(capture.enable_disables == 20u);
    CHECK(capture.enable_arms == 18u);
    CHECK(capture.last_enable == UINT32_C(0x21)); /* empty pass disarms */
    CHECK(capture.request_count == 10u);
    CHECK(capture.request_acks == 10u);

    /* Instruction contract: pure interpreter, 38+26*6+19 = 289 measured
     * across the 10 services. */
    CHECK(executed_after - executed == UINT64_C(289));
    CHECK(interpreted_after - interpreted == executed_after - executed);

    /* Queue end cells (v0746 layout). */
    CHECK(read_u8(session, QUEUE_COUNT) == 0u);
    CHECK(read_u8(session, QUEUE_RCUR) == 3u);
    CHECK(read_u8(session, QUEUE_WCUR) == 3u);
    CHECK(read_u8(session, QUEUE_BYTE_INDEX) == 0xffu);
    CHECK(read_u32(session, QUEUE_STAGED) == UINT32_C(0x00ae101f));

    fprintf(stderr,
            "sound-isr: parked drain bytes=%u ticks=%u popped=%u insns=%llu\n",
            (unsigned)drain.bytes_moved, (unsigned)drain.ticks,
            (unsigned)drain.entries_popped,
            (unsigned long long)(executed_after - executed));

    /* Unarmed control: the drained line is disarmed by the ROM's own empty
     * pass (enable 0x21), so a second service is a zero-work no-op -- no
     * CPU, no bus, no queue motion. */
    memset(&again, 0, sizeof(again));
    CHECK(vf2_session_service_sound_drain(session, &again) == VF2_OK);
    vf2_session_instruction_counts(session, &executed_end, &interpreted_end);
    CHECK(again.armed == 0u);
    CHECK(again.ticks == 0u);
    CHECK(again.bytes_moved == 0u);
    CHECK(executed_end == executed_after);
    CHECK(interpreted_end == interpreted_after);
    CHECK(capture.uart_count == 9u);
    CHECK(capture.enable_count == 38u);
    CHECK(capture.request_count == 10u);

    CHECK(vf2_session_set_memory_observer(session, NULL, NULL) == VF2_OK);
    vf2_session_close(session);
}

/* Leg 2: the pump's shadow-CPU C drainer (established T3 path) must emit
 * the identical byte sequence and queue cells from the same snapshot.
 * The measured divergence -- the C path stops one tick before the ROM
 * empty pass, leaving the enable at 0x421 instead of 0x21 -- is pinned
 * here rather than averaged away. */
static void leg_pump_equivalence(const char *rom, const char *snap)
{
    static bus_capture capture;
    vf2_session *session = NULL;
    vf2_session_audio_pump_report pump;

    memset(&capture, 0, sizeof(capture));
    memset(&pump, 0, sizeof(pump));
    CHECK(vf2_session_open(rom, snap, &session) == VF2_OK);
    if (session == NULL) {
        return;
    }
    CHECK(vf2_session_audio_status(session) == VF2_OK);
    CHECK(vf2_session_set_memory_observer(session, bus_observe, &capture) ==
          VF2_OK);
    CHECK(vf2_session_pump_audio(session, &pump) == VF2_OK);

    CHECK(pump.bytes_moved == 9u);
    CHECK(pump.entries_filed == 3u);
    CHECK(pump.entries_dispatched == 0u);
    CHECK(pump.entries_refused == 3u);
    CHECK(pump.drain_status == VF2_OK);

    CHECK(capture.uart_count == 9u);
    CHECK(memcmp(capture.uart, g_expected_bytes, sizeof(g_expected_bytes)) == 0);

    CHECK(read_u8(session, QUEUE_COUNT) == 0u);
    CHECK(read_u8(session, QUEUE_RCUR) == 3u);
    CHECK(read_u8(session, QUEUE_WCUR) == 3u);
    CHECK(read_u8(session, QUEUE_BYTE_INDEX) == 0xffu);
    CHECK(read_u32(session, QUEUE_STAGED) == UINT32_C(0x00ae101f));

    /* The pinned difference to the real ISR: shadow drain ends armed. */
    CHECK(capture.last_enable == UINT32_C(0x421));

    fprintf(stderr,
            "sound-isr: pump bytes=%u filed=%u uart=%u last_enable=0x%x\n",
            (unsigned)pump.bytes_moved, (unsigned)pump.entries_filed,
            (unsigned)capture.uart_count, (unsigned)capture.last_enable);

    CHECK(vf2_session_set_memory_observer(session, NULL, NULL) == VF2_OK);
    vf2_session_close(session);
}

/* Leg 3: the drain point must not matter. Session A drains at the parked
 * state, then runs frames; session B runs one frame to the vector-12
 * boundary, drains there (nested above the pending frame -- the ISR's ack
 * preserves the frame's request bit 0), then runs the same frames. Each
 * step_oracle call returns at the NEXT vector-12 entry, i.e. with the
 * upcoming frame's body not yet executed, so the digest pairs that must
 * converge are (A after 2 steps, B after 2 steps) and (A after 3, B
 * after 3): "frame body complete" points with the drain at different
 * points on the timeline. */
static void leg_convergence(const char *rom, const char *snap)
{
    vf2_session *a = NULL;
    vf2_session *b = NULL;
    vf2_session_sound_drain_report drain;
    uint32_t d1a = 0u, d2a = 0u, d3a = 0u, d1b = 0u, d2b = 0u;

    memset(&drain, 0, sizeof(drain));
    CHECK(vf2_session_open(rom, snap, &a) == VF2_OK);
    CHECK(vf2_session_open(rom, snap, &b) == VF2_OK);
    if (a == NULL || b == NULL) {
        vf2_session_close(a);
        vf2_session_close(b);
        return;
    }
    /* A: drain first, then three boundary steps. d1a is the frame-1
     * injection point (body pending), d2a and d3a are the completed
     * frame-1 and frame-2 body points. */
    CHECK(vf2_session_service_sound_drain(a, &drain) == VF2_OK);
    CHECK(drain.bytes_moved == 9u);
    CHECK(vf2_session_step_oracle(a, 0u) == VF2_OK);
    CHECK(vf2_session_work_ram_digest(a, &d1a) == VF2_OK);
    CHECK(vf2_session_step_oracle(a, 0u) == VF2_OK);
    CHECK(vf2_session_work_ram_digest(a, &d2a) == VF2_OK);
    CHECK(vf2_session_step_oracle(a, 0u) == VF2_OK);
    CHECK(vf2_session_work_ram_digest(a, &d3a) == VF2_OK);
    /* B: one frame to the vector-12 entry, drain there, then two more
     * steps -- d1b and d2b are the same completed-body points. */
    CHECK(vf2_session_step_oracle(b, 0u) == VF2_OK);
    memset(&drain, 0, sizeof(drain));
    CHECK(vf2_session_service_sound_drain(b, &drain) == VF2_OK);
    CHECK(drain.armed == 1u);
    CHECK(drain.bytes_moved == 9u);
    CHECK(drain.ticks == 10u);
    CHECK(vf2_session_step_oracle(b, 0u) == VF2_OK);
    CHECK(vf2_session_work_ram_digest(b, &d1b) == VF2_OK);
    CHECK(vf2_session_step_oracle(b, 0u) == VF2_OK);
    CHECK(vf2_session_work_ram_digest(b, &d2b) == VF2_OK);

    fprintf(stderr,
            "sound-isr: convergence A=0x%08x/0x%08x/0x%08x "
            "B=0x%08x/0x%08x\n",
            (unsigned)d1a, (unsigned)d2a, (unsigned)d3a,
            (unsigned)d1b, (unsigned)d2b);
    CHECK(d2a == d1b); /* frame-1 body complete */
    CHECK(d3a == d2b); /* frame-2 body complete */

    vf2_session_close(a);
    vf2_session_close(b);
}

/* Leg 4: a session the recovered native runtime advanced refuses the
 * service, untouched (fail closed on unverified state). */
static void leg_native_refusal(const char *rom, const char *snap)
{
    static bus_capture capture;
    vf2_session *session = NULL;
    vf2_session_sound_drain_report drain;
    uint64_t executed = 0u, interpreted = 0u;
    uint64_t executed_after = 0u, interpreted_after = 0u;

    memset(&capture, 0, sizeof(capture));
    memset(&drain, 0, sizeof(drain));
    CHECK(vf2_session_open(rom, snap, &session) == VF2_OK);
    if (session == NULL) {
        return;
    }
    if (vf2_session_step(session, 0u) != VF2_OK) {
        fprintf(stderr,
                "sound-isr: native step refused by this snapshot; "
                "skipping the refusal leg\n");
        vf2_session_close(session);
        return;
    }
    CHECK(vf2_session_set_memory_observer(session, bus_observe, &capture) ==
          VF2_OK);
    vf2_session_instruction_counts(session, &executed, &interpreted);
    CHECK(vf2_session_service_sound_drain(session, &drain) ==
          VF2_ERROR_UNSUPPORTED);
    vf2_session_instruction_counts(session, &executed_after,
                                   &interpreted_after);
    CHECK(executed_after == executed);
    CHECK(interpreted_after == interpreted);
    CHECK(capture.observed == 0u);
    fprintf(stderr, "sound-isr: native-session refusal OK (no work)\n");
    CHECK(vf2_session_set_memory_observer(session, NULL, NULL) == VF2_OK);
    vf2_session_close(session);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s <rom-dir> <park-snapshot> [sixth-snapshot]\n",
                argv[0]);
        return 2;
    }
    {
        FILE *probe = fopen(argv[2], "rb");
        if (probe == NULL) {
            fprintf(stderr, "sound-isr: SKIP, no snapshot at %s\n", argv[2]);
            return 77;
        }
        (void)fclose(probe);
    }
    leg_live_service(argv[1], argv[2]);
    leg_pump_equivalence(argv[1], argv[2]);
    leg_convergence(argv[1], argv[2]);
    if (argc >= 4) {
        FILE *probe = fopen(argv[3], "rb");
        if (probe == NULL) {
            fprintf(stderr, "sound-isr: SKIP native leg, no snapshot at %s\n",
                    argv[3]);
        } else {
            (void)fclose(probe);
            leg_native_refusal(argv[1], argv[3]);
        }
    }
    if (g_fail != 0) {
        fprintf(stderr, "sound-isr: %d failures\n", g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}
