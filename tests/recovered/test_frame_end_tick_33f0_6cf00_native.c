/* ====================================================================
 * 0x6cf00 frame_end_tick_33f0 ctest entry (v0779)
 *
 * Exercises the recovered frame-end tick sibling
 * vf2_hybrid_frame_end_tick_33f0_6cf00_execute which:
 *   1. Reads bus_ptr = *(0x50016c).
 *   2. Increments the u32 at bus+0x33f0 (mirrored to
 *      backup_sram 0x01d033f0 and extended work_ram
 *      0x0059c3f0).
 *   3. Saves r15 = *(sp-4) (i960 SP=r1 NOT r3).
 *   4. Adjusts sp -= 4.
 *   5. Masks r3 to 16 bits (low 16 bits).
 *   6. Stores the masked r3 to 0x005000e4.
 *   7. Calls v0772 (0x5ff7c block_hash_apply) via
 *      vf2_hybrid_block_hash_apply_5ff7c_execute.
 *   8. ret at 0x6d108 -> 0x6d10c.
 *
 * Per vf2i960 disasm (524 B, 2 blocks, 13 instr):
 *   block_0006cf00 (counter inc + sp prep, 7 instr)
 *   block_0006d0f0 (mask + store + call, 6 instr)
 *
 * This is the 0x33f0 counter variant of v0773 (0x6d0a0,
 * which uses 0x33fc). Both call v0772.
 *
 * Test legs vary the initial 0x33f0 counter value and the
 * r3 register; verify the incremented counter is at
 * 0x01d033f0, 0x0059c3f0, and 0x5000e4 holds the low 16.
 *
 * The v0772 sub-recovery calls v0771 which reads a 16-bit
 * table from main_data 0x02000000, so we attach a 128 KB
 * main_data buffer pre-filled with a known identity table
 * to make the hash deterministic.
 *
 * Legs:
 *   A: initial counter = 0, r3 = 0x12345678 -> counter = 1,
 *      r3_masked = 0x5678, stored to 0x5000e4.
 *   B: initial counter = 100, r3 = 0xaabbccdd -> counter = 101,
 *      r3_masked = 0xccdd.
 *   C: initial counter = 0xffffffff, r3 = 0x00000005 -> counter
 *      wraps to 0 (32-bit wrap), r3_masked = 0x0005.
 *   D: pre-set 0x50016c to a different bus_ptr (0x0050a000)
 *      and verify the counter inc targets that location.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0006cf00)
#define POST_RET_IP UINT32_C(0x0006d10c)
#define BUS_PTR_ADDR UINT32_C(0x0050016c)
#define COUNTER_OFFSET UINT32_C(0x33f0)
#define COUNTER_MIRROR_ADDR UINT32_C(0x01d033f0)
#define COUNTER_MIRROR2_ADDR UINT32_C(0x0059c3f0)
#define BUS_PTR_LOW16_ADDR UINT32_C(0x005000e4)
#define MAIN_DATA_SIZE 0x20000u

static uint8_t main_data[MAIN_DATA_SIZE];

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
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t bus_ptr,
    uint32_t initial_counter,
    uint32_t r3_value,
    uint32_t expected_counter,
    uint32_t expected_low16)
{
    vf2_status status;

    /* Set bus_ptr. */
    put_u32(machine, BUS_PTR_ADDR, bus_ptr);
    /* Set initial counter at bus+0x33f0. */
    put_u32(machine, bus_ptr + COUNTER_OFFSET, initial_counter);

    /* Run the recovery. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    /* Set r3 in cpu->registers[3] AFTER enter_procedure
     * (which zeros registers). */
    cpu->registers[3u] = r3_value;
    status = vf2_hybrid_frame_end_tick_33f0_6cf00_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* Verify counter mirrors (note: original code is mirror-only
     * write, does NOT write back to bus[0x33f0]; same pattern
     * as v0773 / v0776's mirror-only writes). */
    CHECK(get_u32(machine, COUNTER_MIRROR_ADDR)
          == expected_counter);
    CHECK(get_u32(machine, COUNTER_MIRROR2_ADDR)
          == expected_counter);

    /* Verify low-16 bus ptr stored. */
    CHECK(get_u32(machine, BUS_PTR_LOW16_ADDR) == expected_low16);
    /* NOTE: cpu->registers[3] is masked to low16 BEFORE the
     * v0772 call, but v0772 (block_hash_apply) overwrites
     * registers[3] internally. We only check the work_mem store,
     * not the live register value. */
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    /* Attach main_data with a simple identity table so v0771
     * (called transitively via v0772) can read 16-bit entries
     * from 0x02000000+offset. */
    {
        uint32_t i;
        for (i = 0u; i < MAIN_DATA_SIZE; ++i) {
            main_data[i] = (uint8_t)(i & 0xffu);
        }
        CHECK(vf2_model2a_attach_main_data(
            &machine, main_data, MAIN_DATA_SIZE) == VF2_OK);
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* Set initial sp (i960 SP=r1) to a valid value so sp-4 read
     * succeeds. Use 0x00501000 (in work_ram). */
    cpu.registers[1u] = UINT32_C(0x00501000);

    /* ====================================================================
     * Leg A: bus_ptr=0x500800, counter=0, r3=0x12345678
     * -> counter = 1, low16 = 0x5678.
     */
    run_leg(&machine, &cpu,
            /*bus_ptr*/UINT32_C(0x00500800),
            /*initial_counter*/0u,
            /*r3*/UINT32_C(0x12345678),
            /*expected_counter*/1u,
            /*expected_low16*/UINT32_C(0x5678));
    printf("  Leg A: counter=0 r3=0x12345678 -> counter=1 "
           "low16=0x5678\n");

    /* ====================================================================
     * Leg B: bus_ptr=0x500800, counter=100, r3=0xaabbccdd
     * -> counter = 101, low16 = 0xccdd.
     */
    run_leg(&machine, &cpu,
            /*bus_ptr*/UINT32_C(0x00500800),
            /*initial_counter*/100u,
            /*r3*/UINT32_C(0xaabbccdd),
            /*expected_counter*/101u,
            /*expected_low16*/UINT32_C(0xccdd));
    printf("  Leg B: counter=100 r3=0xaabbccdd -> counter=101 "
           "low16=0xccdd\n");

    /* ====================================================================
     * Leg C: bus_ptr=0x500800, counter=0xffffffff, r3=0x5
     * -> counter wraps to 0, low16 = 0x0005.
     */
    run_leg(&machine, &cpu,
            /*bus_ptr*/UINT32_C(0x00500800),
            /*initial_counter*/UINT32_C(0xffffffff),
            /*r3*/UINT32_C(0x00000005),
            /*expected_counter*/0u,
            /*expected_low16*/UINT32_C(0x0005));
    printf("  Leg C: counter=0xffffffff r3=0x5 -> counter wraps to 0 "
           "low16=0x5\n");

    /* ====================================================================
     * Leg D: different bus_ptr (0x0050a000) to verify the address
     * is read from 0x50016c each call.
     */
    run_leg(&machine, &cpu,
            /*bus_ptr*/UINT32_C(0x0050a000),
            /*initial_counter*/0u,
            /*r3*/UINT32_C(0xfeedface),
            /*expected_counter*/1u,
            /*expected_low16*/UINT32_C(0xface));
    printf("  Leg D: bus_ptr=0x0050a000 r3=0xfeedface -> "
           "counter=1 low16=0xface\n");

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x6cf00 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x6cf00 frame_end_tick_33f0 (4 legs)\n");
    return 0;
}
