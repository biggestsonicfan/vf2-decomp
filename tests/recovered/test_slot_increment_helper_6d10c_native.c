/* ====================================================================
 * 0x6d10c slot_increment_helper ctest entry (v0768)
 *
 * Exercises the recovered slot-increment leaf
 * vf2_hybrid_slot_increment_helper_6d10c_execute. Per vf2i960
 * disasm (56 B, 3 blocks):
 *  - r7 = max(20, g0 / 1920)
 *  - r3 = *(0x50016c) bus pointer
 *  - r4 = *(r3 + 0x3520 + r7*4) incremented by 1
 *  - mirrored to 0x1d03520[r7*4] and 0x59c520[r7*4]
 *
 * Tests three legs:
 *   A: g0 large (1920*100) -> r7=100, increment slot at bus_ptr+0x3520+400
 *   B: g0 small (100) -> r7=20 (clamped), increment slot at bus_ptr+0x3520+80
 *   C: g0 = 1920*3 + 99 = 5883 -> r7=3, clamps to r7=20 (small index)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0006d10c)
#define POST_RET_IP UINT32_C(0x0006d144)
#define BUS_PTR     UINT32_C(0x0050016c)
#define BUS_BASE    UINT32_C(0x00580200)
#define ARRAY_HI    UINT32_C(0x0059c520)
#define ARRAY_HI_X  UINT32_C(0x01d03520)

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
    CHECK(vf2_model2a_write_u32(m, a, v) == VF2_OK);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(m, a, &v) == VF2_OK);
    return v;
}

static void reset_slot(vf2_model2a *m, uint32_t slot_addr, uint32_t pre) {
    put_u32(m, slot_addr, pre);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;
    const uint32_t G0_REGISTER = (16u + 0u);

    if (vf2_model2a_initialize(&machine) == 0) return 1;

    /* Set the bus pointer. */
    put_u32(&machine, BUS_PTR, BUS_BASE);

    /* ----- Leg A: g0 large -> r7 = g0/1920 = 100 ----- */
    /* Source at (r3 + 0x3520 + 100*4). Destinations at the
     * Model 2A pair (0x59c520 + 100*4) and extended bus pair
     * (0x1d03520 + 100*4). Mirrors pre-set to 0xaaaa/0x5555 so
     * we can confirm they get overwritten. */
    reset_slot(&machine, BUS_BASE + UINT32_C(0x3520) + 100u * 4u, 0x1000u);
    reset_slot(&machine, ARRAY_HI + 100u * 4u, 0xaaaa5555u);
    reset_slot(&machine, ARRAY_HI_X + 100u * 4u, 0x5555aaaau);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    cpu.registers[G0_REGISTER] = UINT32_C(1920) * UINT32_C(100);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_slot_increment_helper_6d10c_execute(
        &machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == POST_RET_IP);
    /* Source is READ-only: the recovery loads from it but
     * does not write back to it. The mirror destinations
     * receive source+1. */
    CHECK(get_u32(&machine, BUS_BASE + UINT32_C(0x3520) + 100u * 4u)
          == 0x1000u);
    CHECK(get_u32(&machine, ARRAY_HI + 100u * 4u) == 0x1001u);
    CHECK(get_u32(&machine, ARRAY_HI_X + 100u * 4u) == 0x1001u);

    /* ----- Leg B: g0 small -> r7 clamps to 20 ----- */
    reset_slot(&machine, BUS_BASE + UINT32_C(0x3520) + 20u * 4u, 0x420u);
    reset_slot(&machine, ARRAY_HI + 20u * 4u, 0x12345678u);
    reset_slot(&machine, ARRAY_HI_X + 20u * 4u, 0x87654321u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    cpu.registers[G0_REGISTER] = UINT32_C(100);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_slot_increment_helper_6d10c_execute(
        &machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == POST_RET_IP);
    CHECK(get_u32(&machine, BUS_BASE + UINT32_C(0x3520) + 20u * 4u)
          == 0x420u);
    CHECK(get_u32(&machine, ARRAY_HI + 20u * 4u) == 0x421u);
    CHECK(get_u32(&machine, ARRAY_HI_X + 20u * 4u) == 0x421u);

    /* ----- Leg C: g0 = 5883 -> r7 = 3 -> clamps to r7 = 20 ----- */
    reset_slot(&machine, BUS_BASE + UINT32_C(0x3520) + 20u * 4u, 0xffu);
    reset_slot(&machine, ARRAY_HI + 20u * 4u, 0u);
    reset_slot(&machine, ARRAY_HI_X + 20u * 4u, 0u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    cpu.registers[G0_REGISTER] = UINT32_C(5883);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_slot_increment_helper_6d10c_execute(
        &machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == POST_RET_IP);
    CHECK(get_u32(&machine, BUS_BASE + UINT32_C(0x3520) + 20u * 4u)
          == 0xffu);
    CHECK(get_u32(&machine, ARRAY_HI + 20u * 4u) == 0x100u);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x6d10c (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x6d10c slot_increment_helper (3 legs)\n");
    return 0;
}
