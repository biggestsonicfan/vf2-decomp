/* ====================================================================
 * 0x65838 kill_osage_evaluate_record ctest entry (v0766)
 *
 * Exercises the recovered kill_osage_evaluate_record
 * vf2_hybrid_kill_osage_evaluate_record_execute.
 *
 * Per vf2i960 disasm (108 B, 7 blocks):
 *  - Read g7 (worker record pointer) from cpu->registers[16+7].
 *  - Initialize r7 = 0.
 *  - Condition gate:
 *      *(g7 + 0xc) == 0x6428c AND *(g7) bit 0 clear AND bit 2 set
 *      AND (*(g7 + 0x128) + g6) >= 0x4268
 *  - YES: *(g7) |= (1<<3); *(0x500164) += 1.
 *  - NO:  *(g7) &= ~(1<<3); g6 += *(g7 + 0x128).
 *  - Ret at 0x658a4.
 *
 * Test verifies all 4 paths:
 *   A: condition fails early  -> NO path, g7 unchanged, g6 += 0
 *   B: condition fails on bit 0 -> NO path
 *   C: condition fails on bit 2 -> NO path
 *   D: condition fails on r7+g6 < 0x4268 -> NO path
 *   E: all conditions met -> YES path, *(g7) bit 3 set, 0x500164++
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x00065838)
#define POST_RET_IP UINT32_C(0x000658a4)
#define MAGIC_0C    UINT32_C(0x0006428c)
#define THRESHOLD   UINT32_C(0x00004268)
#define COUNTER     UINT32_C(0x00500164)
#define G7_REGISTER (16u + 7u)
#define G6_REGISTER (16u + 6u)

static int failures = 0;

#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            fprintf(stderr,                                           \
                "FAILED %s:%d: %s\n",                                 \
                __FILE__, __LINE__, #expr);                           \
            ++failures;                                               \
        }                                                             \
    } while (0)

static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, a, v) == VF2_OK);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(m, a, &v) == VF2_OK);
    return v;
}

static void reset_record(vf2_model2a *m, uint32_t g7_base,
                         uint32_t ptr_minus_one_value,
                         uint32_t flags,
                         uint32_t counter_pre) {
    put_u32(m, g7_base + UINT32_C(0xc), MAGIC_0C);
    put_u32(m, g7_base + UINT32_C(0x128), ptr_minus_one_value);
    put_u32(m, g7_base, flags);
    put_u32(m, COUNTER, counter_pre);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;
    const uint32_t G7_BASE = UINT32_C(0x00580200);
    const uint32_t PRE_COUNTER = UINT32_C(0x12345678);

    if (vf2_model2a_initialize(&machine) == 0) return 1;

    /* ----- Leg A: *(g7 + 0xc) != magic -> NO, g6 += 0 ----- */
    {
        reset_record(&machine, G7_BASE, 100u, (UINT32_C(1) << 2u), PRE_COUNTER);
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        cpu.registers[G7_REGISTER] = G7_BASE;
        cpu.registers[G6_REGISTER] = 50u;
        CHECK(vf2_i960_cpu_enter_procedure(
            &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
        /* *(g7 + 0xc) = 0xdeadbeef (NOT 0x6428c) - should fail first check */
        put_u32(&machine, G7_BASE + UINT32_C(0xc), UINT32_C(0xdeadbeef));
        status = vf2_hybrid_kill_osage_evaluate_record_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        /* flags should have bit 3 cleared (it was clear anyway). */
        CHECK((get_u32(&machine, G7_BASE) & (UINT32_C(1) << 3u)) == 0u);
        /* counter unchanged. */
        CHECK(get_u32(&machine, COUNTER) == PRE_COUNTER);
        /* g6 += 0 (r7 was 0 since *(g7+0x128) wasn't read). */
        CHECK(cpu.registers[G6_REGISTER] == 50u);
    }

    /* ----- Leg B: bit 0 set -> NO, g6 += 0 ----- */
    {
        reset_record(&machine, G7_BASE, 100u,
                     (UINT32_C(1) << 0u) | (UINT32_C(1) << 2u),
                     PRE_COUNTER);
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        cpu.registers[G7_REGISTER] = G7_BASE;
        cpu.registers[G6_REGISTER] = 50u;
        CHECK(vf2_i960_cpu_enter_procedure(
            &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
        status = vf2_hybrid_kill_osage_evaluate_record_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        CHECK(get_u32(&machine, COUNTER) == PRE_COUNTER);
        CHECK(cpu.registers[G6_REGISTER] == 50u);
    }

    /* ----- Leg C: bit 2 clear -> NO, g6 += 0 ----- */
    {
        reset_record(&machine, G7_BASE, 100u, 0u, PRE_COUNTER);
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        cpu.registers[G7_REGISTER] = G7_BASE;
        cpu.registers[G6_REGISTER] = 50u;
        CHECK(vf2_i960_cpu_enter_procedure(
            &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
        status = vf2_hybrid_kill_osage_evaluate_record_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        CHECK(get_u32(&machine, COUNTER) == PRE_COUNTER);
        CHECK(cpu.registers[G6_REGISTER] == 50u);
    }

    /* ----- Leg D: r7 + g6 < 0x4268 -> NO, g6 += 100 ----- */
    {
        reset_record(&machine, G7_BASE, 100u, (UINT32_C(1) << 2u),
                     PRE_COUNTER);
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        cpu.registers[G7_REGISTER] = G7_BASE;
        cpu.registers[G6_REGISTER] = UINT32_C(0x100);
        CHECK(vf2_i960_cpu_enter_procedure(
            &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
        status = vf2_hybrid_kill_osage_evaluate_record_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        /* counter unchanged. */
        CHECK(get_u32(&machine, COUNTER) == PRE_COUNTER);
        /* g6 += 100 (r7 came from *(g7 + 0x128) = 100). */
        CHECK(cpu.registers[G6_REGISTER] == UINT32_C(0x100) + 100u);
        /* flags: bit 3 was clear, stays clear. */
        CHECK((get_u32(&machine, G7_BASE) & (UINT32_C(1) << 3u)) == 0u);
    }

    /* ----- Leg E: all conditions met -> YES, set bit 3, counter++ ----- */
    {
        reset_record(&machine, G7_BASE, 0x5000u,
                     (UINT32_C(1) << 2u), PRE_COUNTER);
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        cpu.registers[G7_REGISTER] = G7_BASE;
        cpu.registers[G6_REGISTER] = 0u;
        CHECK(vf2_i960_cpu_enter_procedure(
            &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
        status = vf2_hybrid_kill_osage_evaluate_record_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        /* Counter incremented. */
        CHECK(get_u32(&machine, COUNTER) == PRE_COUNTER + 1u);
        /* Bit 3 set. */
        CHECK((get_u32(&machine, G7_BASE) & (UINT32_C(1) << 3u))
              == (UINT32_C(1) << 3u));
        /* g6 unchanged (YES path doesn't add). */
        CHECK(cpu.registers[G6_REGISTER] == 0u);
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x65838 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x65838 kill_osage_evaluate_record (5 legs)\n");
    return 0;
}
