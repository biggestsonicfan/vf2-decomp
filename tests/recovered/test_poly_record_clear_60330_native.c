/* ====================================================================
 * 0x60330 poly_record_clear ctest entry (v0774)
 *
 * Exercises the recovered tiny record-clear function
 * vf2_hybrid_poly_record_clear_60330_execute which:
 *   - writes 0 to backup_sram 0x01d03320..0x01d03323 (u32) and
 *     mirror 0x59c320..0x59c323
 *   - writes 0 to 0x01d03324..0x01d03328 (5 bytes) and mirrors
 *   - writes 2 to 0x01d03329..0x01d0332e (6 bytes) and mirrors
 *   - ret -> 0x60410
 *
 * Per vf2i960 disasm (56 instructions, 220 B, 1 block):
 *   00060330  xor      r3, r3, r3
 *   00060334  st       r3, 0x01d03320
 *   0006033c  st       r3, 0x0059c320
 *   00060344-0x6038c  stob    r3, 0x01d03324..0x01d03328 + mirrors
 *   00060394-0x6040c  mov 2, r15; stob 0x01d03329..0x01d0332e + mirrors
 *   0006040c  ret
 *
 * Test legs vary the pre-state of the address range to
 * ensure the recovery overwrites with the expected values:
 *   A: all 0xff (max-value) pre-state
 *   B: all 0xaa pre-state
 *   C: alternating pre-state (byte i = i & 0xff)
 *   D: all 0x00 (already-clear) pre-state
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x00060330)
#define POST_RET_IP UINT32_C(0x00060410)

/* Address range covered by the recovery:
 *   0x01d03320..0x01d03328 (4 + 5 bytes, value 0)
 *   0x01d03329..0x01d0332e (6 bytes, value 2)
 * and their mirrors at 0x59c320..0x59c32e.
 * Total 15 source + 15 mirror = 30 byte addresses. */
#define CLEAR_FIRST  UINT32_C(0x01d03320)
#define CLEAR_LAST   UINT32_C(0x01d03328)
#define TWO_FIRST    UINT32_C(0x01d03329)
#define TWO_LAST     UINT32_C(0x01d0332e)
#define MIRROR_OFFSET UINT32_C(0x59c320 - 0x01d03320)  /* 0x404000 */

static int failures = 0;
#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            fprintf(stderr, "FAILED %s:%d: %s\n",                     \
                __FILE__, __LINE__, #expr);                           \
            ++failures;                                               \
        }                                                             \
    } while (0)

static void put_u8(vf2_model2a *m, uint32_t a, uint8_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}
static void prefill(vf2_model2a *m, uint8_t fill, int alt) {
    uint32_t i;
    /* Pre-fill the full range 0x01d03320..0x01d0332e with a
     * non-clear pattern so the recovery's writes are visible. */
    for (i = CLEAR_FIRST; i <= TWO_LAST; ++i) {
        uint8_t v = alt ? (uint8_t)(i & 0xffu) : fill;
        put_u8(m, i, v);
        put_u8(m, i + MIRROR_OFFSET, v);
    }
}

static void verify_post_state(vf2_model2a *m) {
    uint32_t i;
    /* The first 9 bytes (0x320..0x328) should be 0. */
    for (i = CLEAR_FIRST; i <= CLEAR_LAST; ++i) {
        CHECK(get_u8(m, i) == 0u);
        CHECK(get_u8(m, i + MIRROR_OFFSET) == 0u);
    }
    /* The last 6 bytes (0x329..0x32e) should be 2. */
    for (i = TWO_FIRST; i <= TWO_LAST; ++i) {
        CHECK(get_u8(m, i) == 2u);
        CHECK(get_u8(m, i + MIRROR_OFFSET) == 2u);
    }
}

static void run_leg(
    vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint8_t fill, int alt)
{
    vf2_status status;
    /* Re-prefill addresses in case the previous leg left them
     * with non-zero values (in particular the source got
     * written to 0/2, so re-prefill is needed). */
    prefill(machine, fill, alt);
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_poly_record_clear_60330_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);
    verify_post_state(machine);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ----- Leg A: all 0xff pre-state ----- */
    run_leg(&machine, &cpu, 0xffu, 0);
    printf("  Leg A: 0xff pre-state -> 0/2 ok\n");

    /* ----- Leg B: all 0xaa pre-state ----- */
    run_leg(&machine, &cpu, 0xaau, 0);
    printf("  Leg B: 0xaa pre-state -> 0/2 ok\n");

    /* ----- Leg C: alternating (i & 0xff) pre-state ----- */
    run_leg(&machine, &cpu, 0u, 1);
    printf("  Leg C: alternating pre-state -> 0/2 ok\n");

    /* ----- Leg D: all 0x00 (already-clear) pre-state ----- */
    run_leg(&machine, &cpu, 0u, 0);
    printf("  Leg D: 0x00 pre-state (idempotent) -> 0/2 ok\n");

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x60330 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x60330 poly_record_clear (4 legs)\n");
    return 0;
}
