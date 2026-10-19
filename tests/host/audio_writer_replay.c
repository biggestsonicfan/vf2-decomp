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

static void observer(const vf2_model2a_memory_access *access, void *context)
{
    writer_witness *w = (writer_witness *)context;
    const uint8_t *data;
    uint32_t pc = 0u;
    if (w == NULL || access == NULL ||
        access->kind != VF2_MODEL2A_MEMORY_WRITE ||
        access->data == NULL || access->size != 4u ||
        access->address < UINT32_C(0x00504020) ||
        access->address >= UINT32_C(0x00504060) ||
        (access->address & UINT32_C(3)) != 0u) {
        return;
    }
    if (w->writes == 0u) {
        data = (const uint8_t *)access->data;
        w->slot = access->address;
        w->word = (uint32_t)data[0] |
            ((uint32_t)data[1] << 8u) |
            ((uint32_t)data[2] << 16u) |
            ((uint32_t)data[3] << 24u);
        w->pc_valid =
            vf2_session_current_oracle_instruction_ip(w->session, &pc) ==
            VF2_OK;
        w->pc = w->pc_valid ? pc : 0u;
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
    memset(&w, 0, sizeof(w));
    memset(&access, 0, sizeof(access));
    if (!read_u32("0x43924", &n) || n != UINT32_C(0x43924) ||
        read_u32("-1", &n) || read_u32("4294967296", &n) ||
        read_u32("123garbage", &n)) {
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
    int expect = argc == 7;
    vf2_status status;
    int rc = 1;
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
        return selftest();
    }
    if ((argc != 4 && argc != 7) || !read_u32(argv[3], &input) ||
        (expect && (!read_u32(argv[4], &expected_pc) ||
                    !read_u32(argv[5], &expected_slot) ||
                    !read_u32(argv[6], &expected_word)))) {
        fprintf(stderr,
                "Usage: %s <original-rom-dir> <pre-event.vf2snap> "
                "<original-input-mask> [expected-ip expected-slot expected-word]\n",
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
           "\"natural_fighter_hit_proven\":false,"
           "\"original_uart_irq_proven\":false,"
           "\"hardware_pcm_proven\":false}\n",
           (unsigned)input, (unsigned)before, (unsigned)after,
           (unsigned)witness.writes, (unsigned)witness.slot,
           (unsigned)witness.word, (unsigned)witness.pc,
           witness.pc_valid ? "true" : "false");
    if (expect) {
        if (!witness.pc_valid || witness.pc != expected_pc ||
            witness.slot != expected_slot ||
            witness.word != expected_word) {
            fprintf(stderr,
                    "audio-writer-replay: MISMATCH original i960 "
                    "ip=%08x slot=%08x word=%08x; expected %08x/%08x/%08x\n",
                    (unsigned)witness.pc, (unsigned)witness.slot,
                    (unsigned)witness.word, (unsigned)expected_pc,
                    (unsigned)expected_slot, (unsigned)expected_word);
            goto cleanup;
        }
        puts("MATCH original i960 writer PC / sound-ring slot / command word");
    }
    rc = witness.pc_valid ? 0 : 1;
cleanup:
    (void)vf2_session_set_memory_observer(session, NULL, NULL);
    vf2_session_close(session);
    return rc;
}
