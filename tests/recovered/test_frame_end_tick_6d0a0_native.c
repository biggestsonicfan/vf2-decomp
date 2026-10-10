/* ====================================================================
 * 0x6d0a0 frame_end_tick ctest entry (v0773)
 *
 * Exercises the recovered 27-instruction tick
 * vf2_hybrid_frame_end_tick_6d0a0_execute which:
 *   1. moves r14 (saved return address) into g0
 *   2. calls 0x6d1b4 (v0769 queue dedup-append)
 *   3. reads/writes sp-4 scratch word
 *   4. increments bus[0x33fc] and mirrors to 0x01d033fc + 0x0059c3fc
 *   5. masks the bus pointer's low 16 bits and stores to 0x5000e4
 *   6. calls 0x5ff7c (v0772 block_hash wrapper)
 *   7. ret -> 0x6d10c
 *
 * Per vf2i960 disasm (27 instructions):
 *   0006d0a0  5c80160e  mov      r14, g0
 *   0006d0a4  09000110  call     0x0006d1b4
 *   0006d0a8  90807400  ld       0xfffffffc(sp), g0
 *   0006d0b0  59084904  subo     4, sp, sp
 *   0006d0b4  59084804  addo     4, sp, sp
 *   0006d0b8  92787400  st       r15, 0xfffffffc(sp)
 *   0006d0c0  90783000  ld       0x0050016c, r15
 *   0006d0c8  907bf400  ld       0x000033fc(r15), r15
 *   0006d0d0  597bc801  addo     1, r15, r15
 *   0006d0d4  92783000  st       r15, 0x01d033fc
 *   0006d0dc  92783000  st       r15, 0x0059c3fc
 *   0006d0e4  90787400  ld       0xfffffffc(sp), r15
 *   0006d0ec  59084904  subo     4, sp, sp
 *   0006d0f0  8c783000  lda      0x0000ffff, r15
 *   0006d0f8  581bc083  and      r3, r15, r3
 *   0006d0fc  92183000  st       r3, 0x005000e4
 *   0006d104  09ff2e78  call     0x0005ff7c
 *   0006d108  0a000000  ret
 *
 * Test legs vary the bus pointer and verify the increment+mirror
 * side effects. The hash result is NOT asserted here: the
 * recovery itself writes to 0x5000e4, which lies inside the
 * input range (INPUT_BASE..INPUT_BASE+0x644), so the 0x5ff7c
 * sub-call hashes a *modified* input. The v0772 ctest exercises
 * the same 0x5ff7c->0x9480 chain end-to-end and asserts hash
 * equality.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0006d0a0)
#define POST_RET_IP UINT32_C(0x0006d10c)
#define TABLE_BASE  UINT32_C(0x02000000)
#define PTR_ADDR    UINT32_C(0x0050016c)
#define SP_ADDR     UINT32_C(0x00500c00)
#define SCRATCH_ADDR (SP_ADDR - UINT32_C(4))  /* 0x500bfc */
#define LOW16_ADDR  UINT32_C(0x005000e4)
#define BUMP_BUS    UINT32_C(0x33fc)
#define BUMP_BACKUP UINT32_C(0x01d033fc)
#define BUMP_EXT    UINT32_C(0x0059c3fc)
#define QUEUE_BUS   UINT32_C(0x393c)
#define QUEUE_BACKUP UINT32_C(0x01d0393c)
#define QUEUE_EXT   UINT32_C(0x0059c93c)
#define QUEUE_SLOT  UINT32_C(20)  /* g0 / 1920 clamped to 20 */
#define INPUT_BASE  UINT32_C(0x00500800)
#define INPUT_COUNT UINT32_C(0x644)  /* 1604 (1603 iters + 1 sentinel) */
#define RESULT_ADDR UINT32_C(0x01d03304)

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
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void fill_table_default(uint16_t *table) {
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        table[i] = (uint16_t)(((~(uint32_t)i & 0xffu) << 8) |
                                (i & 0xffu));
    }
}

static void upload_table(uint8_t *main_data_buf,
                         const uint16_t *table) {
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        main_data_buf[i * 2u + 0u] = (uint8_t)(table[i] & 0xffu);
        main_data_buf[i * 2u + 1u] = (uint8_t)((table[i] >> 8) & 0xffu);
    }
}

/* Read a u32 from the test's input array at the given offset
 * (in LE byte order). Used to compute the post-input-write
 * value of a memory location that the input write trampled. */
static uint32_t input_at(const uint8_t *input, uint32_t off) {
    return (uint32_t)input[off]
        | ((uint32_t)input[off + 1u] << 8)
        | ((uint32_t)input[off + 2u] << 16)
        | ((uint32_t)input[off + 3u] << 24);
}

static void run_leg(
    vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint8_t *main_data_buf,
    uint32_t bus_ptr,        /* value written to 0x50016c */
    uint32_t initial_bump,   /* initial value at bus+BUMP_BUS */
    uint32_t initial_queue,  /* initial value at bus+QUEUE_BUS+slot*4 */
    const uint8_t *input,    /* 1604 bytes */
    size_t input_n,
    const uint16_t *table)
{
    vf2_status status;
    uint32_t bump_bus_addr = bus_ptr + BUMP_BUS;
    uint32_t queue_bus_addr = bus_ptr + QUEUE_BUS + QUEUE_SLOT * 4u;
    uint32_t post_input_bump;
    uint32_t post_input_queue;

    /* Reset all per-leg state. */
    put_u32(machine, PTR_ADDR, bus_ptr);
    put_u32(machine, bump_bus_addr, initial_bump);
    put_u32(machine, BUMP_BACKUP, 0u);
    put_u32(machine, BUMP_EXT, 0u);
    put_u32(machine, queue_bus_addr, initial_queue);
    put_u32(machine, QUEUE_BACKUP + QUEUE_SLOT * 4u, 0u);
    put_u32(machine, QUEUE_EXT + QUEUE_SLOT * 4u, 0u);
    put_u32(machine, LOW16_ADDR, 0u);
    put_u32(machine, SCRATCH_ADDR, 0u);
    upload_table(main_data_buf, table);
    {
        size_t i;
        for (i = 0u; i < input_n; ++i) {
            put_u8(machine, INPUT_BASE + (uint32_t)i, input[i]);
        }
    }

    /* The input range (INPUT_BASE..INPUT_BASE+input_n) may
     * overlap the 0x33fc counter and the queue slot locations.
     * Compute the actual post-input value at each location so
     * we can assert the mirror holds (post + 1). */
    if (bump_bus_addr >= INPUT_BASE &&
        bump_bus_addr + UINT32_C(4) <= INPUT_BASE + (uint32_t)input_n) {
        post_input_bump = input_at(input, bump_bus_addr - INPUT_BASE);
    } else {
        post_input_bump = initial_bump;
    }
    if (queue_bus_addr >= INPUT_BASE &&
        queue_bus_addr + UINT32_C(4) <= INPUT_BASE + (uint32_t)input_n) {
        post_input_queue = input_at(input, queue_bus_addr - INPUT_BASE);
    } else {
        post_input_queue = initial_queue;
    }

    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    /* Set sp (r1 in i960) to a work_ram address so the sp-4
     * scratch read/write at SCRATCH_ADDR stays in work_ram. */
    cpu->registers[1u] = SP_ADDR;
    status = vf2_hybrid_frame_end_tick_6d0a0_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* Verify the bus[0x33fc] MIRROR is (post_input_value + 1).
     * Per the disasm 0x6d0a0 only stores to 0x01d033fc and
     * 0x0059c3fc, not back to bus+0x33fc. */
    CHECK(get_u32(machine, BUMP_BACKUP) == post_input_bump + 1u);
    CHECK(get_u32(machine, BUMP_EXT) == post_input_bump + 1u);

    /* Verify the queue dedup-append (0x6d1b4) MIRROR is
     * (post_input_queue + 1). 0x6d1b4 also does NOT write back
     * to the source; it only writes to the mirrors. */
    CHECK(get_u32(machine, QUEUE_BACKUP + QUEUE_SLOT * 4u) ==
          post_input_queue + 1u);
    CHECK(get_u32(machine, QUEUE_EXT + QUEUE_SLOT * 4u) ==
          post_input_queue + 1u);

    /* Verify the low-16-bit bus pointer store. */
    CHECK(get_u32(machine, LOW16_ADDR) == (bus_ptr & 0xffffu));
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    static uint8_t main_data_buf[0x10000];
    static uint8_t input[INPUT_COUNT];
    uint16_t table[256];
    uint32_t i;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    CHECK(vf2_model2a_attach_main_data(
        &machine, main_data_buf, sizeof(main_data_buf)) == VF2_OK);
    fill_table_default(table);
    /* sp-4 scratch must be set up so the recovery's first
     * read at SCRATCH_ADDR returns 0. */
    put_u32(&machine, SCRATCH_ADDR, 0u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ----- Leg A: bus=INPUT_BASE-0x33a8, identity input, bumps 100/200 ----- */
    for (i = 0u; i < INPUT_COUNT - 1u; ++i) {
        input[i] = (uint8_t)(i & 0xffu);
    }
    input[INPUT_COUNT - 1u] = 0u;
    run_leg(&machine, &cpu, main_data_buf,
            INPUT_BASE - UINT32_C(0x33a8), 100u, 200u,
            input, INPUT_COUNT, table);
    printf("  Leg A: bus=0x%x low16=0x%04x ok\n",
           (unsigned)(INPUT_BASE - UINT32_C(0x33a8)),
           (unsigned)get_u32(&machine, LOW16_ADDR));

    /* ----- Leg B: bus=INPUT_BASE-0x33a8+0x1000, 0xaa55 input, bumps 1/2 ----- */
    for (i = 0u; i < INPUT_COUNT - 1u; ++i) {
        input[i] = (i & 1u) ? 0x55u : 0xaau;
    }
    input[INPUT_COUNT - 1u] = 0u;
    run_leg(&machine, &cpu, main_data_buf,
            INPUT_BASE - UINT32_C(0x33a8) + UINT32_C(0x1000),
            1u, 2u,
            input, INPUT_COUNT, table);
    printf("  Leg B: bus=0x%x low16=0x%04x ok\n",
           (unsigned)(INPUT_BASE - UINT32_C(0x33a8) + UINT32_C(0x1000)),
           (unsigned)get_u32(&machine, LOW16_ADDR));

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x6d0a0 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x6d0a0 frame_end_tick (2 legs)\n");
    return 0;
}

