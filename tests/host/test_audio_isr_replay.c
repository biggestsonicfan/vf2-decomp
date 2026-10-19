#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* The ORIGINAL i960 oracle must produce the queue word before EITHER
 * drain path is run. A real vector-15 ISR is then executed on one
 * session; the existing shadow drainer is run on an independent
 * identically stepped session. Nothing is injected into the queue,
 * sample RAM or sound board. This is not fighter-hit causality nor
 * hardware M68K/DSP timing. */
enum {
    UART_ADDR = 0x01c80000u,
    SOUND_COUNT = 0x00504001u,
    SOUND_RCUR = 0x00504002u,
    SOUND_WCUR = 0x00504003u,
    UART_CAP = 64u
};

typedef struct original_witness {
    vf2_session *session;
    uint32_t expected_pc, expected_slot, expected_word;
    uint32_t producer_writes;
    uint32_t matched_writes;
    uint8_t uart[UART_CAP];
    uint32_t uart_count;
    int observing_oracle_frame;
} original_witness;

static int parse_u32(const char *s, uint32_t *value)
{
    unsigned long long n;
    char *end = NULL;
    if (s == NULL || value == NULL || *s == '\0' || *s == '-') {
        return 0;
    }
    errno = 0;
    n = strtoull(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0' || n > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)n;
    return 1;
}

static uint32_t little_word(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static void original_bus_observer(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    original_witness *w = (original_witness *)context;
    if (w == NULL || access == NULL ||
        access->kind != VF2_MODEL2A_MEMORY_WRITE ||
        access->data == NULL) {
        return;
    }
    if (w->observing_oracle_frame && access->size == 4u &&
        access->address >= UINT32_C(0x00504020) &&
        access->address < UINT32_C(0x00504060) &&
        (access->address & 3u) == 0u) {
        uint32_t pc = 0u;
        ++w->producer_writes;
        if (vf2_session_current_oracle_instruction_ip(w->session, &pc) ==
                VF2_OK &&
            pc == w->expected_pc &&
            access->address == w->expected_slot &&
            little_word((const uint8_t *)access->data) == w->expected_word) {
            ++w->matched_writes;
        }
    }
    if (!w->observing_oracle_frame &&
        access->address == UART_ADDR &&
        access->size >= 1u && w->uart_count < UART_CAP) {
        w->uart[w->uart_count++] = *(const uint8_t *)access->data;
    }
}

static int read_queue(vf2_session *session, uint8_t *cells)
{
    return vf2_session_read_work_ram(
        session, UINT32_C(0x00504000), cells, 4u) == VF2_OK;
}

static int bytes_match(const uint8_t *a, const uint8_t *b, size_t count)
{
    return count == 0u || memcmp(a, b, count) == 0;
}

static void print_bytes(const char *label, const uint8_t *bytes, uint32_t n)
{
    uint32_t i;
    printf("%s=", label);
    for (i = 0u; i < n; ++i) printf("%02x", (unsigned)bytes[i]);
    putchar('\n');
}

static int selftest(void)
{
    static const uint8_t three[3] = {0x9eu, 0x0au, 0x7fu};
    uint32_t n = 0u;
    uint8_t cells[4] = {0u, 1u, 2u, 3u};
    if (!parse_u32("0x43924", &n) || n != UINT32_C(0x43924) ||
        parse_u32("-1", &n) ||
        parse_u32("4294967296", &n) ||
        parse_u32("garbage", &n) ||
        !bytes_match(three, three, sizeof(three)) ||
        bytes_match(three, cells, sizeof(three)) ||
        little_word(cells) != UINT32_C(0x03020100)) {
        return 1;
    }
    puts("audio-isr-replay: ROM-free parser/byte test PASS");
    return 0;
}

int main(int argc, char **argv)
{
    vf2_session *original = NULL;
    vf2_session *shadow = NULL;
    original_witness witness;
    original_witness shadow_witness;
    vf2_session_sound_drain_report real;
    vf2_session_audio_pump_report pump;
    uint8_t before[4] = {0}, after_original[4] = {0}, after_shadow[4] = {0};
    uint32_t input, pc, slot, word;
    vf2_status so, ss, ro, rs;
    int rc = 1;
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
    if (argc != 7 ||
        !parse_u32(argv[3], &input) ||
        !parse_u32(argv[4], &pc) ||
        !parse_u32(argv[5], &slot) ||
        !parse_u32(argv[6], &word)) {
        fprintf(stderr, "usage: %s <original-rom-dir> <pre-event.vf2snap> "
                "<input-mask> <expected-producer-pc> <expected-slot> "
                "<expected-word>\n", argv[0]);
        return 2;
    }
    if (pc == 0u || (pc & 3u) != 0u ||
        slot < UINT32_C(0x00504020) ||
        slot >= UINT32_C(0x00504060) ||
        (slot & 3u) != 0u) {
        fprintf(stderr, "audio-isr-replay: invalid producer witness\n");
        return 2;
    }
    memset(&witness, 0, sizeof(witness));
    memset(&shadow_witness, 0, sizeof(shadow_witness));
    memset(&real, 0, sizeof(real));
    memset(&pump, 0, sizeof(pump));
    if (vf2_session_open(argv[1], argv[2], &original) != VF2_OK ||
        vf2_session_open(argv[1], argv[2], &shadow) != VF2_OK ||
        original == NULL || shadow == NULL) {
        fprintf(stderr, "audio-isr-replay: owner ROM or pre-event snapshot unavailable\n");
        rc = 2;
        goto done;
    }
    witness.session = original;
    witness.expected_pc = pc;
    witness.expected_slot = slot;
    witness.expected_word = word;
    shadow_witness.session = shadow;
    shadow_witness.expected_pc = pc;
    shadow_witness.expected_slot = slot;
    shadow_witness.expected_word = word;
    if (vf2_session_set_memory_observer(
            original, original_bus_observer, &witness) != VF2_OK ||
        vf2_session_set_memory_observer(
            shadow, original_bus_observer, &shadow_witness) != VF2_OK ||
        !read_queue(original, before)) {
        fprintf(stderr, "audio-isr-replay: observer or original queue not readable\n");
        goto done;
    }
    witness.observing_oracle_frame = 1;
    shadow_witness.observing_oracle_frame = 1;
    so = vf2_session_step_oracle(original, input);
    ss = vf2_session_step_oracle(shadow, input);
    witness.observing_oracle_frame = 0;
    shadow_witness.observing_oracle_frame = 0;
    if (so != VF2_OK || ss != VF2_OK) {
        fprintf(stderr, "audio-isr-replay: frame failure original=%d shadow=%d\n",
                (int)so, (int)ss);
        goto done;
    }
    if (witness.matched_writes == 0u ||
        shadow_witness.matched_writes == 0u ||
        witness.producer_writes != shadow_witness.producer_writes) {
        fprintf(stderr, "audio-isr-replay: original witness not repeated "
                "(real=%u/%u shadow=%u/%u)\n",
                (unsigned)witness.matched_writes,
                (unsigned)witness.producer_writes,
                (unsigned)shadow_witness.matched_writes,
                (unsigned)shadow_witness.producer_writes);
        goto done;
    }
    ro = vf2_session_service_sound_drain(original, &real);
    rs = vf2_session_pump_audio(shadow, &pump);
    if (!read_queue(original, after_original) ||
        !read_queue(shadow, after_shadow)) {
        fprintf(stderr, "audio-isr-replay: post-drain queue read failed\n");
        goto done;
    }

    printf("ORIGINAL_IP=%08x RING_SLOT=%08x RING_WORD=%08x "
           "GAME_INPUT=%08x WRITES=%u PRE_COUNT=%u\n",
           (unsigned)pc, (unsigned)slot, (unsigned)word, (unsigned)input,
           (unsigned)witness.producer_writes, (unsigned)before[1]);
    printf("ORIGINAL_ISR status=%d armed=%u ticks=%u bytes=%u popped=%u "
           "uart_writes=%u post_count=%u rcur=%u wcur=%u\n",
           (int)ro, (unsigned)real.armed, (unsigned)real.ticks,
           (unsigned)real.bytes_moved, (unsigned)real.entries_popped,
           (unsigned)witness.uart_count, (unsigned)after_original[1],
           (unsigned)after_original[2], (unsigned)after_original[3]);
    printf("SHADOW_PUMP status=%d drain=%d bytes=%u filed=%u "
           "uart_writes=%u post_count=%u rcur=%u wcur=%u\n",
           (int)rs, (int)pump.drain_status, (unsigned)pump.bytes_moved,
           (unsigned)pump.entries_filed, (unsigned)shadow_witness.uart_count,
           (unsigned)after_shadow[1], (unsigned)after_shadow[2],
           (unsigned)after_shadow[3]);
    print_bytes("ORIGINAL_ISR_UART", witness.uart, witness.uart_count);
    print_bytes("SHADOW_UART", shadow_witness.uart, shadow_witness.uart_count);

    if (ro != VF2_OK || rs != VF2_OK || !real.armed ||
        real.bytes_moved == 0u ||
        real.bytes_moved != witness.uart_count ||
        pump.bytes_moved != shadow_witness.uart_count ||
        real.bytes_moved != pump.bytes_moved ||
        !bytes_match(witness.uart, real.bytes, real.bytes_moved) ||
        !bytes_match(witness.uart, shadow_witness.uart, real.bytes_moved) ||
        !bytes_match(witness.uart, pump.observed_uart, pump.observed_uart_count) ||
        pump.observed_uart_count != real.bytes_moved ||
        after_original[1] != after_shadow[1] ||
        after_original[2] != after_shadow[2] ||
        after_original[3] != after_shadow[3]) {
        fprintf(stderr, "DIFFERENTIAL original ISR / shadow sound drain FAILED\n");
        goto done;
    }
    printf("MATCH original i960 game command -> vector15 UART "
           "vs shadow UART bytes=%u\n", (unsigned)real.bytes_moved);
    rc = 0;
done:
    if (original != NULL) {
        (void)vf2_session_set_memory_observer(original, NULL, NULL);
        vf2_session_close(original);
    }
    if (shadow != NULL) {
        (void)vf2_session_set_memory_observer(shadow, NULL, NULL);
        vf2_session_close(shadow);
    }
    return rc;
}
