/* ====================================================================
 * 0x5427c poly_record_load ctest entry (v0776)
 *
 * Exercises the recovered polygon record loader
 * vf2_hybrid_poly_record_load_5427c_execute which:
 *   1. zero-fills 256 bytes at 0x01d03100 (dst) via
 *      'cmpdeco 0, r15, r15; bl 0x542a4'
 *   2. runs an 18-iteration loop (r10 = 0..17):
 *      - r10 < 6: call 0x94d0 (timer_fuse); mask to 5 bits;
 *                 retry while bit set in r6 bitmap
 *      - r10 >= 6: use r10 as the slot index
 *      - common body: writes 9 bytes per iter to
 *        dst + 16*r10 (offsets 0, 4, 10)
 *   3. ret at 0x54314 -> 0x54318
 *
 * Per vf2i960 disasm (156 B, 8 blocks):
 *   block_0005427c (init)        block_000542e0 (common body)
 *   block_000542a4 (zero-fill)   block_00054314 (ret)
 *   block_000542b0 (loop top)
 *   block_000542b8 (prealloc)
 *   block_000542c8 (alloc retry)
 *   block_000542d4 (alloc done)
 *
 * Test setup:
 *   src       at 0x0201ea24  (in main_data, 18 chunks x 12 B)
 *   poly_tab  at 0x0201eafc  (in main_data, 32 entries x 4 B)
 *   poly_data at 0x00500800  (in work_ram, 32 entries x 4 B)
 *   dst       at 0x01d03100  (in backup_sram, 256 B + spillover)
 *   timers    at 0xF00000..0xF0000C (t0=1, t1=t2=t3=0)
 *   fuse      at 0x00500098  (initial 0; accumulates per call)
 *
 * With timers t0=1, t1=t2=t3=0:
 *   call N to 0x94d0: state = N * 0x10, return (state>>4) = N.
 *   So alloc iters r10=0..5 use slots 1,2,3,4,5,6 respectively.
 *   Prealloc iters r10=6..17 use slot = r10 (slot 6 is shared
 *   with alloc iter 5; setbit is idempotent).
 *
 * The poly_table values are ADDRESSES; the recovery dereferences
 * them to get the poly data word. So poly_tab[i] must equal
 * poly_data_base + i*4 (= 0x00500800 + i*4) for the deref to
 * land in our poly data buffer.
 *
 * Test legs:
 *   A: zero src/zero poly -> verifies zero-fill + alloc path +
 *       body writes zero everywhere.
 *   B: distinct src and poly -> verifies body fills struct at
 *       offsets 0/4/10 correctly across both alloc and prealloc
 *       paths.
 *   C: pre-fill dst with 0xAA sentinel -> verifies zero-fill
 *       overwrites.
 *   D: deadbeef src, distinct poly, post-call 0x500098 = 0x60
 *       (state after 6 alloc calls).
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0005427c)
#define POST_RET_IP UINT32_C(0x00054318)
#define DST_BASE    UINT32_C(0x01d03100)
#define SRC_BASE    UINT32_C(0x0201ea24)
#define POLY_TABLE  UINT32_C(0x0201eafc)
#define POLY_DATA_BASE UINT32_C(0x00500800)
#define FUSE_ADDR   UINT32_C(0x00500098)
#define TIMER_BASE  UINT32_C(0x00F00000)

/* 128 KB main_data: covers src at offset 0x1ea24 and
 * poly_table at 0x1eafc. (Both addresses are within the first
 * 128 KB of the main_data region 0x02000000..0x021fffff.) */
#define MAIN_DATA_SIZE 0x20000u
#define SRC_OFFSET_IN_MAIN_DATA (SRC_BASE - UINT32_C(0x02000000))
#define POLY_TABLE_OFFSET_IN_MAIN_DATA (POLY_TABLE - UINT32_C(0x02000000))

#define SRC_CHUNK_BYTES 12u
#define N_STRUCTURES 18u

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
static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static void put_u32_md(uint8_t *main_data, uint32_t off, uint32_t v) {
    main_data[off + 0u] = (uint8_t)(v & 0xffu);
    main_data[off + 1u] = (uint8_t)((v >> 8) & 0xffu);
    main_data[off + 2u] = (uint8_t)((v >> 16) & 0xffu);
    main_data[off + 3u] = (uint8_t)((v >> 24) & 0xffu);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void set_timers(vf2_model2a *m, uint32_t t0, uint32_t t1,
                       uint32_t t2, uint32_t t3) {
    put_u32(m, TIMER_BASE + 0u,  t0);
    put_u32(m, TIMER_BASE + 4u,  t1);
    put_u32(m, TIMER_BASE + 8u,  t2);
    put_u32(m, TIMER_BASE + 12u, t3);
}

/* Determine which slot each iter r10 ends up using.
 *
 * Timer setup (set_timers): t0=1, t1=t2=t3=0 -> 0x94d0
 * accumulates 1<<4 = 0x10 per call. Starting from pre_fuse=0:
 *   call 1: state = 0x10, return 0x10>>4 = 1, r5 = 1
 *   call 2: state = 0x20, return 2
 *   ...
 *   call 6: state = 0x60, return 6
 *
 * So alloc iters r10 = 0..5 use slots 1..6 respectively.
 * Prealloc iters r10 = 6..17 use slot = r10.
 * Slot 6 is used by both alloc (r10=5) and prealloc (r10=6);
 * setbit is idempotent so that's fine. */
static uint32_t expected_slot(uint32_t r10) {
    if (r10 < 6u) {
        return r10 + 1u;
    }
    return r10;
}

/* Run a single test leg with given inputs.
 *
 * main_data     : 128 KB main_data buffer (src + poly_table live here)
 * src_word[]    : 18 32-bit words for src base+0 (offset 0)
 * src_byte6[]   : 18 bytes for src base+6
 * poly_data[]   : 32 32-bit words for the poly data buffer in work_ram
 * pre_dst_sentinel : if non-zero, pre-fill dst[0..255] with this value
 *                    before the recovery runs (verifies zero-fill overwrites)
 */
static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint8_t *main_data,
    const uint32_t *src_word,
    const uint8_t *src_byte6,
    const uint32_t *poly_data,
    uint8_t pre_dst_sentinel)
{
    vf2_status status;
    uint32_t i;

    /* Reset and reattach. */
    vf2_model2a_shutdown(machine);
    CHECK(vf2_model2a_initialize(machine) != 0);
    CHECK(vf2_model2a_attach_main_data(
        machine, main_data, MAIN_DATA_SIZE) == VF2_OK);

    /* Optionally pre-fill dst with sentinel. */
    if (pre_dst_sentinel != 0u) {
        for (i = 0u; i < 256u; ++i) {
            put_u8(machine, DST_BASE + i, pre_dst_sentinel);
        }
    }

    /* Fill src chunks (18 x 12 B) in main_data. */
    for (i = 0u; i < N_STRUCTURES; ++i) {
        uint32_t off = SRC_OFFSET_IN_MAIN_DATA + i * SRC_CHUNK_BYTES;
        put_u32_md(main_data, off, src_word[i]);
        main_data[off + 4u] = 0u;
        main_data[off + 5u] = 0u;
        main_data[off + 6u] = src_byte6[i];
        main_data[off + 7u] = 0u;
        main_data[off + 8u] = 0u;
        main_data[off + 9u] = 0u;
        main_data[off + 10u] = 0u;
        main_data[off + 11u] = 0u;
    }

    /* Fill polygon_table (32 x 4 B) in main_data. Each entry
     * is the ADDRESS of poly_data[i] in work_ram. */
    for (i = 0u; i < 32u; ++i) {
        put_u32_md(main_data,
                   POLY_TABLE_OFFSET_IN_MAIN_DATA + i * 4u,
                   POLY_DATA_BASE + i * 4u);
    }

    /* Upload poly_data into work_ram. */
    for (i = 0u; i < 32u; ++i) {
        put_u32(machine, POLY_DATA_BASE + i * 4u, poly_data[i]);
    }

    /* Set timers t0=1, others=0 so 0x94d0 returns 1,2,3,4,5,6
     * across the 6 alloc calls. */
    set_timers(machine, 1u, 0u, 0u, 0u);

    /* Reset fuse to 0 so first 0x94d0 call returns 1. */
    put_u32(machine, FUSE_ADDR, 0u);

    /* Run the recovery. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_poly_record_load_5427c_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* Verify dst layout. */
    for (i = 0u; i < N_STRUCTURES; ++i) {
        uint32_t base = DST_BASE + i * 16u;
        uint32_t slot = expected_slot(i);
        uint32_t expected_poly = poly_data[slot];
        uint32_t got0 = get_u32(machine, base + 0u);
        uint32_t got4 = get_u32(machine, base + 4u);
        uint8_t got10 = get_u8(machine, base + 10u);
        CHECK(got0 == src_word[i]);
        CHECK(got4 == expected_poly);
        CHECK(got10 == src_byte6[i]);
    }
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    static uint8_t main_data[MAIN_DATA_SIZE];

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ====================================================================
     * Leg A: zero src, zero poly_data -> all dst writes are 0.
     */
    {
        uint32_t src_word[N_STRUCTURES];
        uint8_t  src_byte6[N_STRUCTURES];
        uint32_t poly_data[32];
        uint32_t i;
        for (i = 0u; i < N_STRUCTURES; ++i) {
            src_word[i] = 0u;
            src_byte6[i] = 0u;
        }
        for (i = 0u; i < 32u; ++i) poly_data[i] = 0u;
        run_leg(&machine, &cpu, main_data, src_word, src_byte6,
                poly_data, 0u);
        printf("  Leg A: zero src/zero poly -> 18 struct fills\n");
    }

    /* ====================================================================
     * Leg B: distinct src and poly -> verifies body writes across
     * both alloc (r10=0..5) and prealloc (r10=6..17) paths.
     */
    {
        uint32_t src_word[N_STRUCTURES];
        uint8_t  src_byte6[N_STRUCTURES];
        uint32_t poly_data[32];
        uint32_t i;
        for (i = 0u; i < N_STRUCTURES; ++i) {
            src_word[i] = (i + 1u) * UINT32_C(0x01010101);
            src_byte6[i] = (uint8_t)((i * 17u) & 0xffu);
        }
        for (i = 0u; i < 32u; ++i) {
            poly_data[i] = UINT32_C(0x12345670) + i * UINT32_C(0x100);
        }
        run_leg(&machine, &cpu, main_data, src_word, src_byte6,
                poly_data, 0u);
        printf("  Leg B: i*0x01010101 src, 0x1234567x poly -> "
               "18 struct fills\n");
    }

    /* ====================================================================
     * Leg C: pre-fill dst with sentinel 0xAA -> zero-fill must
     * overwrite.
     */
    {
        uint32_t src_word[N_STRUCTURES];
        uint8_t  src_byte6[N_STRUCTURES];
        uint32_t poly_data[32];
        uint32_t i;
        for (i = 0u; i < N_STRUCTURES; ++i) {
            src_word[i] = (i + 1u) * UINT32_C(0x02020202);
            src_byte6[i] = (uint8_t)((i * 23u) & 0xffu);
        }
        for (i = 0u; i < 32u; ++i) {
            poly_data[i] = UINT32_C(0xa5a5a5a5) ^
                           (i * UINT32_C(0x11111111));
        }
        run_leg(&machine, &cpu, main_data, src_word, src_byte6,
                poly_data, 0xaau);
        printf("  Leg C: dst pre-filled 0xAA, distinct src/poly -> "
               "zero-fill overwrites\n");
    }

    /* ====================================================================
     * Leg D: verify post-call fuse state = 0x60 (state after 6
     * alloc calls with t0=1, others=0).
     */
    {
        uint32_t src_word[N_STRUCTURES];
        uint8_t  src_byte6[N_STRUCTURES];
        uint32_t poly_data[32];
        uint32_t i;
        for (i = 0u; i < N_STRUCTURES; ++i) {
            src_word[i] = UINT32_C(0xdeadbeef) ^ i;
            src_byte6[i] = (uint8_t)(0xc0u + i);
        }
        for (i = 0u; i < 32u; ++i) {
            poly_data[i] = UINT32_C(0xfeedface) - i;
        }
        run_leg(&machine, &cpu, main_data, src_word, src_byte6,
                poly_data, 0u);
        uint32_t fuse = get_u32(&machine, FUSE_ADDR);
        CHECK(fuse == 0x60u);
        printf("  Leg D: deadbeef src -> post-call 0x500098 = 0x%lx "
               "(expected 0x60)\n", (unsigned long)fuse);
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x5427c (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x5427c poly_record_load (4 legs)\n");
    return 0;
}