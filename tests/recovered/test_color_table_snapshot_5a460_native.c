/* ====================================================================
 * 0x5a460 color_table_snapshot ctest entry (v0778)
 *
 * Exercises the recovered bus-table snapshot
 * vf2_hybrid_color_table_snapshot_5a460_execute which:
 *   1. Reads bus_ptr = *(0x50016c).
 *   2. Reads 7 bytes from bus-relative offsets 0x3356, 0x3359,
 *      0x3357, 0x335a, 0x3358, 0x335b, 0x335c (a "shuffled"
 *      snapshot of the 7-byte window 0x3356..0x335c).
 *   3. Stores them to work_ram 0x500234..0x50023a.
 *   4. Refuses the sub-call to 0x2b4c (color_table_rebuild)
 *      with VF2_ERROR_UNSUPPORTED, with cpu->ip set to the
 *      post-call address 0x5a510.
 *
 * Per vf2i960 disasm (176 B, 1 block):
 *   block_0005a460 (26 instr): read 7 bytes from bus region,
 *     store to 0x500234..0x50023a, call 0x2b4c.
 *
 * Test legs vary the 7 source bytes; the dst 0x500234..0x50023a
 * must contain the bytes in the order the snapshot reads them
 * (NOT the natural order 0x3356, 0x3357, ..., 0x335c).
 *
 * Legs:
 *   A: identity bytes (0..6 at bus+0x3356..0x335c) -> verify
 *      dst[0x234..0x23a] = (0,1,2,3,4,5,6) but in the snapshot
 *      order: 0, 3, 1, 4, 2, 5, 6.
 *   B: 0xaa repeating (all 7 bytes)
 *   C: distinct bytes (10..16)
 *   D: shuffled order with all-zero bus region (verify default
 *      zeros are passed through unchanged)
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0005a460)
#define POST_CALL_IP UINT32_C(0x0005a510)
#define BUS_PTR_ADDR UINT32_C(0x0050016c)
#define SRC_BASE_OFFSET UINT32_C(0x3356)
#define DST_BASE_ADDR UINT32_C(0x00500234)

static int failures = 0;
#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            fprintf(stderr, "FAILED %s:%d: %s\n",                     \
                __FILE__, __LINE__, #expr);                           \
            ++failures;                                               \
        }                                                             \
    } while (0)

static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static void put_u8(vf2_model2a *m, uint32_t a, uint8_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

/* The 7 byte offsets in the order the snapshot reads them.
 * (NOT the natural byte order 0x3356, 0x3357, ..., 0x335c.) */
static const uint32_t SRC_OFFSETS[7] = {
    0x3356, 0x3359, 0x3357, 0x335a, 0x3358, 0x335b, 0x335c
};

static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t bus_ptr,
    const uint8_t src_bytes[7])
{
    vf2_status status;
    uint32_t i;

    /* Set bus_ptr. */
    put_u32(machine, BUS_PTR_ADDR, bus_ptr);
    /* Fill the 7 source bytes. */
    for (i = 0u; i < 7u; ++i) {
        put_u8(machine, bus_ptr + SRC_OFFSETS[i], src_bytes[i]);
    }
    /* Optionally clear dst 0x500234..0x50023a. */
    for (i = 0u; i < 7u; ++i) {
        put_u8(machine, DST_BASE_ADDR + i, 0xaau);
    }

    /* Run the recovery. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_CALL_IP) == VF2_OK);
    status = vf2_hybrid_color_table_snapshot_5a460_execute(
        machine, cpu);
    /* Refused sub-call returns VF2_ERROR_UNSUPPORTED. */
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->ip == POST_CALL_IP);

    /* Verify dst bytes. */
    for (i = 0u; i < 7u; ++i) {
        uint8_t got = get_u8(machine, DST_BASE_ADDR + i);
        CHECK(got == src_bytes[i]);
    }
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ====================================================================
     * Leg A: identity bytes 0..6 at the natural 0x3356..0x335c.
     * The snapshot reads them in shuffled order:
     *   [0] = 0x3356, [1] = 0x3359, [2] = 0x3357, [3] = 0x335a,
     *   [4] = 0x3358, [5] = 0x335b, [6] = 0x335c
     * So dst bytes = (0, 3, 1, 4, 2, 5, 6).
     */
    {
        uint8_t src_bytes[7];
        uint32_t i;
        for (i = 0u; i < 7u; ++i) {
            src_bytes[i] = (uint8_t)i;
        }
        run_leg(&machine, &cpu,
                /*bus_ptr*/UINT32_C(0x00500800),
                src_bytes);
        printf("  Leg A: identity (shuffled snapshot) -> "
               "dst[0..6] = (0,3,1,4,2,5,6)\n");
    }

    /* ====================================================================
     * Leg B: 0xAA repeating -> all dst bytes = 0xAA.
     */
    {
        uint8_t src_bytes[7];
        uint32_t i;
        for (i = 0u; i < 7u; ++i) {
            src_bytes[i] = 0xaau;
        }
        run_leg(&machine, &cpu,
                /*bus_ptr*/UINT32_C(0x00500800),
                src_bytes);
        printf("  Leg B: 0xAA repeating -> dst[0..6] = all 0xAA\n");
    }

    /* ====================================================================
     * Leg C: distinct bytes 10..16 -> dst[0..6] = (10, 13, 11, 14, 12,
     * 15, 16).
     */
    {
        uint8_t src_bytes[7];
        uint32_t i;
        for (i = 0u; i < 7u; ++i) {
            src_bytes[i] = (uint8_t)(10u + i);
        }
        run_leg(&machine, &cpu,
                /*bus_ptr*/UINT32_C(0x00500800),
                src_bytes);
        printf("  Leg C: 10..16 distinct -> dst[0..6] = "
               "(10,13,11,14,12,15,16)\n");
    }

    /* ====================================================================
     * Leg D: different bus_ptr value to verify the address is
     * read from 0x50016c each call (not hardcoded).
     */
    {
        uint8_t src_bytes[7];
        uint32_t i;
        for (i = 0u; i < 7u; ++i) {
            src_bytes[i] = (uint8_t)(0xc0u + i);
        }
        run_leg(&machine, &cpu,
                /*bus_ptr*/UINT32_C(0x0050a000),
                src_bytes);
        printf("  Leg D: bus_ptr=0x0050a000 -> dst[0..6] = "
               "(0xc0,0xc3,0xc1,0xc4,0xc2,0xc5,0xc6)\n");
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x5a460 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x5a460 color_table_snapshot (4 legs)\n");
    return 0;
}