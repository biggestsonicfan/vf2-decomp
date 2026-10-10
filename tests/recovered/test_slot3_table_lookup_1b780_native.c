/* ====================================================================
 * 0x1b780 slot3_table_lookup ctest entry (v0777)
 *
 * Exercises the recovered 3-bit slot table lookup
 * vf2_hybrid_slot3_table_lookup_1b780_execute which:
 *   1. Reads r3, r4, r5, r6, r7 from cpu->registers.
 *   2. Three gates decide between two paths:
 *      - cmpobe r3, r7 -> default (0x282) if r3 > r7
 *      - cmpobne r3, r4 -> lookup (call 0x94d0) if r3 != r4
 *      - cmpobl r5, r6 -> default (0x282) if r5 < r6
 *      - All three must "match the original" for lookup path:
 *        r3 <= r7 AND r3 != r4 AND r5 >= r6.
 *   3. Lookup path: call 0x94d0 (timer_fuse), mask g0 to 3 bits,
 *      read 16-bit word from main_data at 0x0201cb8c + g0*2,
 *      return it as g0.
 *   4. Default path: return 0x282.
 *
 * Per vf2i960 disasm (20 B, 1 block):
 *   block_0001b780 (lookup body, 4 instr)
 *     0001b780  call     0x000094d0
 *     0001b784  and      7, g0, g0
 *     0001b788  ldos     0x0201cb8c[g0*2], g0
 *     0001b790  ret
 *
 * The 3 cmpob gates at 0x1b774..0x1b77c are fall-through
 * "tail" gates for a function that branches into 0x1b780 via
 * b 0x1b780 (at 0x1b778). The fall-through happens only
 * when all 3 gates match the original intent.
 *
 * Test legs:
 *   A: lookup path - r3=0, r4=1 (so r3!=r4), r5=2, r6=1 (so
 *      r5>=r6), r7=0 (so r3<=r7). Set timers so 0x94d0 returns
 *      slot N=3 -> lookup table[3*2] = 0xbeef.
 *   B: default via cmpobe (r3 > r7): r3=5, r7=4 -> 0x282.
 *   C: default via cmpobne (r3 == r4): r3=r4=7 -> 0x282.
 *   D: default via cmpobl (r5 < r6): r5=1, r6=2 -> 0x282.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0001b780)
#define POST_RET_IP UINT32_C(0x0001b79c)
#define TABLE_ADDR  UINT32_C(0x0201cb8c)
#define TABLE_OFFSET_IN_MAIN_DATA (TABLE_ADDR - UINT32_C(0x02000000))
#define TABLE_ENTRY_COUNT 8u
#define FUSE_ADDR   UINT32_C(0x00500098)
#define TIMER_BASE  UINT32_C(0x00F00000)
#define MAIN_DATA_SIZE 0x20000u

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
static void put_u16_md(uint8_t *main_data, uint32_t off, uint16_t v) {
    main_data[off + 0u] = (uint8_t)(v & 0xffu);
    main_data[off + 1u] = (uint8_t)((v >> 8) & 0xffu);
}
static void set_timers(vf2_model2a *m, uint32_t t0, uint32_t t1,
                       uint32_t t2, uint32_t t3) {
    put_u32(m, TIMER_BASE + 0u,  t0);
    put_u32(m, TIMER_BASE + 4u,  t1);
    put_u32(m, TIMER_BASE + 8u,  t2);
    put_u32(m, TIMER_BASE + 12u, t3);
}

/* Run a leg with r3..r7 set as specified. Pre-load the table
 * with the expected values and timers to make 0x94d0 return
 * a known value (for the lookup path). */
static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint8_t *main_data,
    const uint16_t *table,
    uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7,
    uint32_t pre_fuse, uint32_t t0, uint32_t t1, uint32_t t2,
    uint32_t t3, uint32_t expected_g0)
{
    vf2_status status;
    uint32_t i;
    uint16_t g0_16 = 0u;

    vf2_model2a_shutdown(machine);
    CHECK(vf2_model2a_initialize(machine) != 0);
    CHECK(vf2_model2a_attach_main_data(
        machine, main_data, MAIN_DATA_SIZE) == VF2_OK);

    /* Fill the 8-entry table at 0x0201cb8c. */
    for (i = 0u; i < TABLE_ENTRY_COUNT; ++i) {
        put_u16_md(main_data, TABLE_OFFSET_IN_MAIN_DATA + i * 2u,
                   table[i]);
    }

    /* Set timers and pre-fuse so 0x94d0 returns a known value
     * (for the lookup-path leg A). */
    set_timers(machine, t0, t1, t2, t3);
    put_u32(machine, FUSE_ADDR, pre_fuse);

    /* Drive cpu state. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    cpu->registers[3u] = r3;
    cpu->registers[4u] = r4;
    cpu->registers[5u] = r5;
    cpu->registers[6u] = r6;
    cpu->registers[7u] = r7;

    status = vf2_hybrid_slot3_table_lookup_1b780_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    g0_16 = (uint16_t)cpu->registers[VF2_I960_G0_REGISTER + 0u];
    CHECK(g0_16 == (uint16_t)expected_g0);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    static uint8_t main_data[MAIN_DATA_SIZE];

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ====================================================================
     * Leg A: lookup path.
     * r3=0, r4=1 (r3!=r4), r5=2, r6=1 (r5>=r6), r7=0 (r3<=r7).
     * Timers t0=3, others=0 -> 0x94d0 contribution = 3<<4 = 0x30.
     * Pre-fuse=0 -> state=0x30 -> return 0x30>>4 = 3. So slot = 3.
     * Expected table[3] = 0xbeef.
     */
    {
        uint16_t table[TABLE_ENTRY_COUNT];
        uint32_t i;
        for (i = 0u; i < TABLE_ENTRY_COUNT; ++i) {
            table[i] = (uint16_t)(0xc000u + i);
        }
        table[3] = 0xbeefu;
        run_leg(&machine, &cpu, main_data, table,
               /*r3*/0u, /*r4*/1u, /*r5*/2u, /*r6*/1u, /*r7*/0u,
               /*pre_fuse*/0u, /*t0*/3u, /*t1*/0u, /*t2*/0u,
               /*t3*/0u,
               /*expected_g0*/0xbeefu);
        printf("  Leg A: lookup path -> slot 3 = 0xbeef\n");
    }

    /* ====================================================================
     * Leg B: default via cmpobe (r3 > r7).
     * r3=5, r7=4. Other gates can pass or fail, we hit default
     * first via cmpobe. Expected: 0x282.
     */
    {
        uint16_t table[TABLE_ENTRY_COUNT];
        uint32_t i;
        for (i = 0u; i < TABLE_ENTRY_COUNT; ++i) {
            table[i] = (uint16_t)(0x1234u + i);
        }
        run_leg(&machine, &cpu, main_data, table,
               /*r3*/5u, /*r4*/0u, /*r5*/2u, /*r6*/1u, /*r7*/4u,
               /*pre_fuse*/0u, /*t0*/0u, /*t1*/0u, /*t2*/0u,
               /*t3*/0u,
               /*expected_g0*/0x282u);
        printf("  Leg B: default via cmpobe -> 0x282\n");
    }

    /* ====================================================================
     * Leg C: default via cmpobne (r3 == r4).
     * r3=r4=7. cmpobe fails (7 > 6 false), cmpobne fails (7 == 7
     * means no branch), cmpobl irrelevant. Default 0x282.
     */
    {
        uint16_t table[TABLE_ENTRY_COUNT];
        uint32_t i;
        for (i = 0u; i < TABLE_ENTRY_COUNT; ++i) {
            table[i] = (uint16_t)(0xdeadu + i);
        }
        run_leg(&machine, &cpu, main_data, table,
               /*r3*/7u, /*r4*/7u, /*r5*/2u, /*r6*/1u, /*r7*/8u,
               /*pre_fuse*/0u, /*t0*/0u, /*t1*/0u, /*t2*/0u,
               /*t3*/0u,
               /*expected_g0*/0x282u);
        printf("  Leg C: default via cmpobne -> 0x282\n");
    }

    /* ====================================================================
     * Leg D: default via cmpobl (r5 < r6).
     * r3=1, r4=2, r5=1, r6=2 (r5<r6). cmpobe fails (1<7 false),
     * cmpobne fails (1!=2 true -> branch taken to 0x1b780),
     * cmpobl true -> default 0x282.
     */
    {
        uint16_t table[TABLE_ENTRY_COUNT];
        uint32_t i;
        for (i = 0u; i < TABLE_ENTRY_COUNT; ++i) {
            table[i] = (uint16_t)(0xfeedu + i);
        }
        run_leg(&machine, &cpu, main_data, table,
               /*r3*/1u, /*r4*/2u, /*r5*/1u, /*r6*/2u, /*r7*/7u,
               /*pre_fuse*/0u, /*t0*/0u, /*t1*/0u, /*t2*/0u,
               /*t3*/0u,
               /*expected_g0*/0x282u);
        printf("  Leg D: default via cmpobl -> 0x282\n");
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x1b780 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x1b780 slot3_table_lookup (4 legs)\n");
    return 0;
}