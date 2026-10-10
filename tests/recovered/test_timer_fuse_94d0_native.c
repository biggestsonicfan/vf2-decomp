/* ====================================================================
 * 0x94d0 timer_fuse ctest entry (v0775)
 *
 * Exercises the recovered 4-timer fuse
 * vf2_hybrid_timer_fuse_94d0_execute which:
 *   1. reads the persistent fuse state from 0x500098
 *   2. loads the 4 Model 2A timer values (0xF00000..0xF0000C)
 *   3. accumulates r15 += (timer[0]<<4) | (timer[1]<<8)
 *                        | (timer[2]<<12) | (timer[3]<<16)
 *   4. writes the accumulated value back to 0x500098
 *   5. returns g0 = (r15 >> 4) & 0xffff
 *
 * Per vf2i960 disasm (22 instructions, 88 B, 1 block):
 *   000094d0  ld       0x00500098, r15
 *   000094d8  shlo     20, 15, r14
 *   000094dc  ld       (r14), r13
 *   000094e0  shlo     4, r13, r13
 *   000094e4  addi     r13, r15, r15
 *   000094e8-0x9508  (same pattern for timer[1..3])
 *   0000950c  st       r15, 0x00500098
 *   00009514  shro     4, r15, g0
 *   00009520  and      0xffff, g0, g0
 *   00009524  ret
 *
 * Test legs vary the 4 timer values to cover:
 *   A: all 4 timers = 0
 *   B: all 4 timers = the default reload (0x07800f0f)
 *   C: timer[i] = i+1 (so the 4-bit nibbles are distinct)
 *   D: pre-load 0x500098 to a non-zero value and assert it
 *      participates in the accumulation
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x000094d0)
#define POST_RET_IP UINT32_C(0x00009528)
#define FUSE_ADDR   UINT32_C(0x00500098)
#define TIMER_BASE  UINT32_C(0x00F00000)
#define TIMER_RELOAD UINT32_C(0x07800f0f)

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

static uint32_t run_leg(
    vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3,
    uint32_t pre_fuse)
{
    vf2_status status;
    /* Reset the fuse state and set the 4 timer values. */
    put_u32(machine, FUSE_ADDR, pre_fuse);
    put_u32(machine, TIMER_BASE + 0u,  t0);
    put_u32(machine, TIMER_BASE + 4u,  t1);
    put_u32(machine, TIMER_BASE + 8u,  t2);
    put_u32(machine, TIMER_BASE + 12u, t3);

    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_timer_fuse_94d0_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* Compute expected values per the disasm:
     *   new_fuse = pre_fuse
     *            + (t0 << 4) + (t1 << 8) + (t2 << 12) + (t3 << 16)
     *   g0 = (new_fuse >> 4) & 0xffff  */
    {
        uint32_t new_fuse = pre_fuse
            + (t0 << 4) + (t1 << 8) + (t2 << 12) + (t3 << 16);
        uint16_t expected_g0 = (uint16_t)((new_fuse >> 4) & 0xffffu);
        uint32_t actual_fuse = get_u32(machine, FUSE_ADDR);
        uint32_t actual_g0 =
            cpu->registers[VF2_I960_G0_REGISTER + 0u];
        CHECK(actual_fuse == new_fuse);
        CHECK((uint16_t)actual_g0 == expected_g0);
        return actual_g0;
    }
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ----- Leg A: all timers = 0, pre_fuse = 0 -> g0 = 0 ----- */
    {
        uint32_t g = run_leg(&machine, &cpu, 0u, 0u, 0u, 0u, 0u);
        printf("  Leg A: timers=0 pre=0 -> g0=0x%x\n", (unsigned)g);
    }

    /* ----- Leg B: all timers = TIMER_RELOAD (default), pre_fuse = 0 ----- */
    {
        uint32_t g = run_leg(&machine, &cpu,
                             TIMER_RELOAD, TIMER_RELOAD,
                             TIMER_RELOAD, TIMER_RELOAD, 0u);
        printf("  Leg B: timers=0x%08x (reload) pre=0 -> g0=0x%x\n",
               (unsigned)TIMER_RELOAD, (unsigned)g);
    }

    /* ----- Leg C: timer[i] = i+1, pre_fuse = 0 ----- */
    {
        uint32_t g = run_leg(&machine, &cpu, 1u, 2u, 3u, 4u, 0u);
        printf("  Leg C: timers=1,2,3,4 pre=0 -> g0=0x%x\n", (unsigned)g);
    }

    /* ----- Leg D: pre_fuse != 0, timers = 0 ----- */
    {
        uint32_t g = run_leg(&machine, &cpu, 0u, 0u, 0u, 0u, 0xdeadbeefu);
        printf("  Leg D: timers=0 pre=0xdeadbeef -> g0=0x%x\n",
               (unsigned)g);
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x94d0 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x94d0 timer_fuse (4 legs)\n");
    return 0;
}
