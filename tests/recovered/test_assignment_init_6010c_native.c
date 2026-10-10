/* ====================================================================
 * 0x6010c assignment_init ctest entry (v0781)
 *
 * Exercises the recovered assignment-block INITIALIZE
 * vf2_hybrid_assignment_init_6010c_execute which:
 *   1. Reads g6 (input, production value 6).
 *   2. Writes hardcoded defaults to backup_sram
 *      0x01d03340..0x01d0335c and extended_work_ram
 *      0x0059c340..0x0059c35c (mirror):
 *        0x340..0x341 = 2 (MATCH COUNT)
 *        0x342..0x34e = 1 (11 per-round defaults)
 *        0x34f       = 15 (stage width)
 *        0x352       = 0xa0 (160, ENERGY MAX)
 *        0x354       = 0xc8 (200, ENERGY MAX 2)
 *        0x350,0x351 = 0 (region header)
 *        0x356..0x358 = 0x40 (64, video bias)
 *        0x359..0x35b = g6+31 = 0x25 (37, video gain)
 *        0x35c       = 31 (scroll)
 *   3. Calls 0x5a460 (v0778 color_table_snapshot) which
 *      writes 7 bytes to work_ram 0x500234..0x50023a.
 *
 * Per vf2i960 disasm (548 B, 1 block, 134 instr):
 *   block_0006010c (linear, no branches, ends with call+ret)
 *
 * Legs:
 *   A: g6=6, pre-fill 0x01d03340..0x0059c35c with 0xff ->
 *      verify all 29+29 bytes match the documented defaults.
 *   B: g6=4, pre-fill with 0xff -> verify gain = 4+31=35=0x23.
 *   C: g6=0, pre-fill with 0xff -> verify gain = 0+31=0x1f.
 *   D: g6=10, pre-fill with 0xff -> verify gain = 10+31=41=0x29.
 *   Also verify color_table_snapshot was called by checking
 *     0x500234..0x50023a are non-zero (set by 0x5a460).
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0006010c)
#define POST_RET_IP UINT32_C(0x00060330)
#define BS_BASE     UINT32_C(0x01d03340)
#define EWR_BASE    UINT32_C(0x0059c340)
#define SNAP_BASE   UINT32_C(0x00500234)
#define BUS_PTR_ADDR UINT32_C(0x0050016c)
#define BUS_PTR_VAL  UINT32_C(0x00500000)
#define BLOCK_SIZE  29u
/* src offsets used by v0778: 0x3356, 0x3359, 0x3357, 0x335a,
 * 0x3358, 0x335b, 0x335c. */
static const uint32_t SNAP_SRC_OFFSETS[7] = {
    0x3356u, 0x3359u, 0x3357u, 0x335au,
    0x3358u, 0x335bu, 0x335cu
};
static const uint8_t SNAP_SRC_BYTES[7] = {
    0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u
};

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
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}
static uint16_t get_u16(vf2_model2a *m, uint32_t a) {
    uint16_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t g6)
{
    vf2_status status;
    uint32_t i;

    /* Pre-fill BS_BASE..BS_BASE+29 and EWR_BASE..EWR_BASE+29
       with 0xff so any unwritten byte is detected. */
    for (i = 0u; i < BLOCK_SIZE; ++i) {
        put_u8(machine, BS_BASE + i, 0xffu);
        put_u8(machine, EWR_BASE + i, 0xffu);
    }
    /* Pre-fill snapshot dst with 0xaa, src with
       SNAP_SRC_BYTES so v0778's writes are verifiable. */
    for (i = 0u; i < 7u; ++i) {
        put_u8(machine, SNAP_BASE + i, 0xaau);
        put_u8(machine, BUS_PTR_VAL + SNAP_SRC_OFFSETS[i],
               SNAP_SRC_BYTES[i]);
    }
    /* Set bus_ptr (used by v0778 color_table_snapshot). */
    put_u32(machine, BUS_PTR_ADDR, BUS_PTR_VAL);

    /* Set g6 in the global register file. */
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = g6;

    /* Run the recovery. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_assignment_init_6010c_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* === BS region === */
    /* 0x340..0x341 = 2 (MATCH COUNT) */
    CHECK(get_u8(machine, BS_BASE + 0u) == 2u);
    CHECK(get_u8(machine, BS_BASE + 1u) == 2u);
    /* 0x342..0x34e = 1 (11 per-round defaults) */
    for (i = 2u; i <= 14u; ++i) {
        CHECK(get_u8(machine, BS_BASE + i) == 1u);
    }
    /* 0x34f = 15 (stage width) */
    CHECK(get_u8(machine, BS_BASE + 15u) == 15u);
    /* 0x350 = 0, 0x351 = 0 (region header) */
    CHECK(get_u8(machine, BS_BASE + 16u) == 0u);
    CHECK(get_u8(machine, BS_BASE + 17u) == 0u);
    /* 0x352 = 0xa0 (160) -- read as u16 */
    CHECK(get_u16(machine, BS_BASE + 18u) == 0xa0u);
    /* 0x354 = 0xc8 (200) */
    CHECK(get_u16(machine, BS_BASE + 20u) == 0xc8u);
    /* 0x356..0x358 = 0x40 (64, video bias) */
    CHECK(get_u8(machine, BS_BASE + 22u) == 0x40u);
    CHECK(get_u8(machine, BS_BASE + 23u) == 0x40u);
    CHECK(get_u8(machine, BS_BASE + 24u) == 0x40u);
    /* 0x359..0x35b = (uint8_t)(g6+31) */
    {
        uint8_t expect = (uint8_t)((g6 + 31u) & 0xffu);
        CHECK(get_u8(machine, BS_BASE + 25u) == expect);
        CHECK(get_u8(machine, BS_BASE + 26u) == expect);
        CHECK(get_u8(machine, BS_BASE + 27u) == expect);
    }
    /* 0x35c = 31 (scroll) */
    CHECK(get_u8(machine, BS_BASE + 28u) == 31u);

    /* === EWR mirror region: same bytes as BS === */
    for (i = 0u; i < 16u; ++i) {
        CHECK(get_u8(machine, EWR_BASE + i) ==
              get_u8(machine, BS_BASE + i));
    }
    CHECK(get_u8(machine, EWR_BASE + 16u) == 0u);
    CHECK(get_u8(machine, EWR_BASE + 17u) == 0u);
    CHECK(get_u16(machine, EWR_BASE + 18u) == 0xa0u);
    CHECK(get_u16(machine, EWR_BASE + 20u) == 0xc8u);
    for (i = 22u; i <= 24u; ++i) {
        CHECK(get_u8(machine, EWR_BASE + i) == 0x40u);
    }
    {
        uint8_t expect = (uint8_t)((g6 + 31u) & 0xffu);
        CHECK(get_u8(machine, EWR_BASE + 25u) == expect);
        CHECK(get_u8(machine, EWR_BASE + 26u) == expect);
        CHECK(get_u8(machine, EWR_BASE + 27u) == expect);
    }
    CHECK(get_u8(machine, EWR_BASE + 28u) == 31u);

    /* === color_table_snapshot ran: 0x500234..0x50023a
     * must equal SNAP_SRC_BYTES. The recovery chain
     * tolerates v0778's refused sub-call (0x2b4c) because
     * the 7 byte stores happen BEFORE the refusal. */
    for (i = 0u; i < 7u; ++i) {
        CHECK(get_u8(machine, SNAP_BASE + i) ==
              SNAP_SRC_BYTES[i]);
    }
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    run_leg(&machine, &cpu, 6u);
    printf("  Leg A: g6=6 -> gain=37=0x25, defaults written\n");

    run_leg(&machine, &cpu, 4u);
    printf("  Leg B: g6=4 -> gain=35=0x23\n");

    run_leg(&machine, &cpu, 0u);
    printf("  Leg C: g6=0 -> gain=31=0x1f\n");

    run_leg(&machine, &cpu, 10u);
    printf("  Leg D: g6=10 -> gain=41=0x29\n");

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x6010c (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x6010c assignment_init (4 legs)\n");
    return 0;
}
