#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/session.h"
#include "vf2/status.h"

/* Independent ORIGINAL-i960 replay of a locally saved pre-event snapshot.
 * No ring/command/sound board state is injected. One original oracle frame
 * is stepped and the first aligned, original 4-byte bus write to the i960
 * sound-command ring is captured with the PRE-INSTRUCTION PC.
 * This does NOT demonstrate a fighter hit, original UART ISR timing,
 * original M68K IRQ reception, or hardware PCM fidelity. */
typedef struct writer_witness {
    vf2_session *session;
    uint32_t writes;
    uint32_t slot;
    uint32_t word;
    uint32_t pc;
    int pc_valid;
    int expected_enabled;
    uint32_t expected_pc;
    uint32_t expected_slot;
    uint32_t expected_word;
    uint32_t matched_write_index;
    int matched;
} writer_witness;

static int read_u32(const char *str, uint32_t *value)
{
    char *end = NULL;
    unsigned long long n;
    if (str == NULL || value == NULL || *str == '\0' || *str == '-') {
        return 0;
    }
    errno = 0;
    n = strtoull(str, &end, 0);
    if (errno != 0 || end == str || *end != '\0' ||
        n > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)n;
    return 1;
}

/* Optional exact UART transcript, no inferred packet delimiter.
 * This independently runs the ORIGINAL ROM vector-15 sound ISR,
 * but its external IRQ is explicitly serviced by the session model.
 * It is not passive capture of physical Model 2A UART timing. */
static int hex_nibble(char value)
{
    if (value >= '0' && value <= '9') return (int)(value - '0');
    if (value >= 'a' && value <= 'f') return (int)(value - 'a') + 10;
    if (value >= 'A' && value <= 'F') return (int)(value - 'A') + 10;
    return -1;
}

static int parse_uart_hex(
    const char *text,
    uint8_t out[VF2_SESSION_SOUND_DRAIN_MAX_BYTES],
    size_t *out_length
)
{
    size_t length;
    size_t i;
    if (text == NULL || out == NULL || out_length == NULL) return 0;
    length = strlen(text);
    if (length == 0u || (length % 2u) != 0u ||
        length / 2u > VF2_SESSION_SOUND_DRAIN_MAX_BYTES) return 0;
    for (i = 0u; i < length; i += 2u) {
        int a = hex_nibble(text[i]);
        int b = hex_nibble(text[i + 1u]);
        if (a < 0 || b < 0) return 0;
        out[i / 2u] = (uint8_t)((a << 4u) | b);
    }
    *out_length = length / 2u;
    return 1;
}

static void observer(const vf2_model2a_memory_access *access, void *context)
{
    writer_witness *w = (writer_witness *)context;
    const uint8_t *data;
    uint32_t pc = 0u;
    uint32_t word;
    int valid;
    if (w == NULL || access == NULL ||
        access->kind != VF2_MODEL2A_MEMORY_WRITE ||
        access->data == NULL || access->size != 4u ||
        access->address < UINT32_C(0x00504020) ||
        access->address >= UINT32_C(0x00504060) ||
        (access->address & UINT32_C(3)) != 0u) {
        return;
    }
    data = (const uint8_t *)access->data;
    word = (uint32_t)data[0] |
        ((uint32_t)data[1] << 8u) |
        ((uint32_t)data[2] << 16u) |
        ((uint32_t)data[3] << 24u);
    valid = vf2_session_current_oracle_instruction_ip(w->session, &pc) ==
        VF2_OK;
    if (w->writes == 0u) {
        w->slot = access->address;
        w->word = word;
        w->pc_valid = valid;
        w->pc = valid ? pc : 0u;
    }
    /* A frame may start with reset/zeroing stores (e.g. original
     * IP 0x100) BEFORE the real command writer IP 0x43924.
     * The independently requested witness may be any original
     * aligned ring write, not necessarily the frame's first. */
    if (!w->matched && w->expected_enabled && valid &&
        pc == w->expected_pc &&
        access->address == w->expected_slot &&
        word == w->expected_word) {
        w->matched = 1;
        w->matched_write_index = w->writes;
    }
    if (w->writes < UINT32_MAX) {
        ++w->writes;
    }
}

static int selftest(void)
{
    writer_witness w;
    vf2_model2a_memory_access access;
    static const uint8_t word[4] = {0x42u, 0x23u, 0xadu, 0x00u};
    uint32_t n = 0u;
    uint8_t expected_uart[VF2_SESSION_SOUND_DRAIN_MAX_BYTES] = {0u};
    size_t expected_len = 0u;
    memset(&w, 0, sizeof(w));
    memset(&access, 0, sizeof(access));
    if (!read_u32("0x43924", &n) || n != UINT32_C(0x43924) ||
        read_u32("-1", &n) || read_u32("4294967296", &n) ||
        read_u32("123garbage", &n) ||
        !parse_uart_hex("9E0a7f", expected_uart, &expected_len) ||
        expected_len != 3u || expected_uart[0] != 0x9eu ||
        expected_uart[1] != 0x0au || expected_uart[2] != 0x7fu ||
        parse_uart_hex("9e0a7", expected_uart, &expected_len) ||
        parse_uart_hex("XX", expected_uart, &expected_len) ||
        parse_uart_hex("", expected_uart, &expected_len)) {
        return 1;
    }
    access.kind = VF2_MODEL2A_MEMORY_READ;
    access.address = UINT32_C(0x00504020);
    access.data = word;
    access.size = 4u;
    observer(&access, &w);
    if (w.writes != 0u) return 1;
    access.kind = VF2_MODEL2A_MEMORY_WRITE;
    access.address = UINT32_C(0x00504021);
    observer(&access, &w);
    if (w.writes != 0u) return 1;
    access.address = UINT32_C(0x00504020);
    observer(&access, &w);
    /* Synthetic callback has no oracle session; PC must be unproven. */
    if (w.writes != 1u || w.slot != UINT32_C(0x00504020) ||
        w.word != UINT32_C(0x00ad2342) || w.pc_valid != 0 ||
        w.pc != 0u) {
        return 1;
    }
    puts("audio-writer-replay: ROM-free validation PASS");
    return 0;
}

int main(int argc, char **argv)
{
    vf2_session *session = NULL;
    writer_witness witness;
    uint8_t before = 0u;
    uint8_t after = 0u;
    uint32_t input = 0u;
    uint32_t expected_pc = 0u;
    uint32_t expected_slot = 0u;
    uint32_t expected_word = 0u;
    int expect = argc == 7 || argc == 8;
    int compare_original_isr = argc == 8;
    uint8_t expected_uart[VF2_SESSION_SOUND_DRAIN_MAX_BYTES] = {0u};
    size_t expected_uart_length = 0u;
    vf2_status status;
    int rc = 1;
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
        return selftest();
    }
    if ((argc != 4 && argc != 7 && argc != 8) ||
        !read_u32(argv[3], &input) ||
        (expect && (!read_u32(argv[4], &expected_pc) ||
                    !read_u32(argv[5], &expected_slot) ||
                    !read_u32(argv[6], &expected_word))) ||
        (compare_original_isr &&
         !parse_uart_hex(argv[7], expected_uart,
                         &expected_uart_length))) {
        fprintf(stderr,
                "Usage: %s <original-rom-dir> <pre-event.vf2snap> "
                "<original-input-mask> "
                "[expected-ip expected-slot expected-word [expected-isr-hex]]\n",
                argv[0]);
        return 2;
    }
    memset(&witness, 0, sizeof(witness));
    status = vf2_session_open(argv[1], argv[2], &session);
    if (status != VF2_OK) {
        fprintf(stderr, "audio-writer-replay: failed loading original snapshot (%d)\n",
                (int)status);
        return 2;
    }
    witness.session = session;
    witness.expected_enabled = expect;
    witness.expected_pc = expected_pc;
    witness.expected_slot = expected_slot;
    witness.expected_word = expected_word;
    status = vf2_session_set_memory_observer(session, observer, &witness);
    if (status != VF2_OK ||
        vf2_session_read_work_ram(session, UINT32_C(0x00504001),
                                  &before, 1u) != VF2_OK) {
        fprintf(stderr, "audio-writer-replay: observer/read unavailable\n");
        goto cleanup;
    }
    status = vf2_session_step_oracle(session, input);
    if (status != VF2_OK) {
        fprintf(stderr, "audio-writer-replay: original frame failed (%d)\n",
                (int)status);
        goto cleanup;
    }
    if (vf2_session_read_work_ram(session, UINT32_C(0x00504001),
                                  &after, 1u) != VF2_OK) {
        fprintf(stderr, "audio-writer-replay: post-frame read failed\n");
        goto cleanup;
    }
    if (witness.writes == 0u) {
        fprintf(stderr,
                "audio-writer-replay: no original i960 ring writes in frame "
                "(before=%u after=%u); no sound event asserted\n",
                (unsigned)before, (unsigned)after);
        rc = expect ? 1 : 77;
        goto cleanup;
    }
    printf("{\"type\":\"original_i960_writer_replay\","
           "\"origin\":\"one_original_oracle_frame\","
           "\"input\":%u,\"queue_before\":%u,\"queue_after\":%u,"
           "\"original_i960_ring_writes\":%u,"
           "\"original_i960_first_ring_slot\":%u,"
           "\"original_i960_first_ring_word\":%u,"
           "\"original_i960_first_writer_pc\":%u,"
           "\"original_i960_writer_pc_proven\":%s,"
           "\"matched_expected_writer\":%s,"
           "\"matched_write_index\":%u,"
           "\"natural_fighter_hit_proven\":false,"
           "\"original_uart_irq_proven\":false,"
           "\"hardware_pcm_proven\":false}\n",
           (unsigned)input, (unsigned)before, (unsigned)after,
           (unsigned)witness.writes, (unsigned)witness.slot,
           (unsigned)witness.word, (unsigned)witness.pc,
           witness.pc_valid ? "true" : "false",
           witness.matched ? "true" : "false",
           (unsigned)witness.matched_write_index);
    if (expect) {
        if (!witness.matched) {
            fprintf(stderr,
                    "audio-writer-replay: no matching original i960 "
                    "write at any of %u ring writes; expected %08x/%08x/%08x\n",
                    (unsigned)witness.writes, (unsigned)expected_pc,
                    (unsigned)expected_slot, (unsigned)expected_word);
            goto cleanup;
        }
        printf("MATCH original i960 writer PC / sound-ring slot / "
               "command word, write-index=%u/%u\n",
               (unsigned)witness.matched_write_index,
               (unsigned)witness.writes);
    }
    if (compare_original_isr) {
        vf2_session_sound_drain_report drain;
        size_t i;
        memset(&drain, 0, sizeof(drain));
        status = vf2_session_service_sound_drain(session, &drain);
        if (status != VF2_OK) {
            fprintf(stderr,
                    "audio-writer-replay: original ROM ISR service failed "
                    "(%d); no UART equivalence asserted\n", (int)status);
            goto cleanup;
        }
        printf("ORIGINAL ROM vector-15 serviced armed=%u ticks=%u "
               "bytes=%u popped=%u uart=",
               (unsigned)drain.armed, (unsigned)drain.ticks,
               (unsigned)drain.bytes_moved,
               (unsigned)drain.entries_popped);
        for (i = 0u; i < drain.bytes_moved &&
                      i < VF2_SESSION_SOUND_DRAIN_MAX_BYTES; ++i) {
            printf("%02x", (unsigned)drain.bytes[i]);
        }
        putchar('\n');
        if (drain.bytes_moved != expected_uart_length ||
            memcmp(drain.bytes, expected_uart,
                   expected_uart_length) != 0) {
            fprintf(stderr,
                    "audio-writer-replay: original ISR UART bytes did not "
                    "match expected exact transcript\n");
            goto cleanup;
        }
        puts("MATCH original i960 queue producer -> original ROM "
             "vector-15 ISR UART transcript (explicit IRQ service)");
    }
    rc = expect ? witness.matched ? 0 : 1 : witness.pc_valid ? 0 : 1;
cleanup:
    (void)vf2_session_set_memory_observer(session, NULL, NULL);
    vf2_session_close(session);
    return rc;
}
