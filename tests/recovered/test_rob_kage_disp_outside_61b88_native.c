/* ====================================================================
 * 0x61b88 rob_kage_disp_outside ctest entry (v0784)
 *
 * Exercises the recovered shipped-symbol function
 * vf2_hybrid_rob_kage_disp_outside_61b88_execute, the first
 * recovery from the v0783 87 KB gap characterisation:
 *
 * Per vf2i960 disasm (24 B, 3 blocks, 6 instr):
 *   00061b88  ldob     0x0000002a(g7), r3
 *   00061b8c  cmpobne  0, r3, 0x00061b9c
 *   00061b90  ld       0x00000684(g7), g4
 *   00061b94  mov      2, g5
 *   00061b98  call     0x00061ba0
 *   00061b9c  ret
 *
 * Legs:
 *   A1: flag byte 0x01 (nonzero) -> path A admitted: VF2_OK,
 *       ip == 0x61ba0 (post-ret marker, numerically equal to the
 *       refused callee entry), g4/g5 untouched, +2 instructions.
 *   A2: flag byte 0xff (nonzero, not 1) -> same admitted shape.
 *       Proves the gate is !=0 rather than ==1.
 *   B:  flag byte 0x00 -> path B refused: VF2_ERROR_UNSUPPORTED,
 *       ip == 0x61b9c (own ret slot, cf04 doctrine),
 *       g4 == word at g7+0x684, g5 == 2, +4 instructions.
 *   C:  negative control: cpu->ip != entry -> VF2_ERROR_UNSUPPORTED
 *       with no side effects (entry guard holds).
 */

#include <stdint.h>
#include <stdio.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x00061b88)
#define POST_RET_IP UINT32_C(0x00061ba0)
#define REFUSE_IP   UINT32_C(0x00061b9c)
#define G7_BASE     UINT32_C(0x00510000)
#define FLAG_ADDR   UINT32_C(0x0051002a)
#define WORD_ADDR   UINT32_C(0x00510684)
#define TEST_WORD   UINT32_C(0x12345678)
#define G4_SENTINEL UINT32_C(0xdeadbeef)
#define G5_SENTINEL UINT32_C(0xcafef00d)

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

/* Prime one leg: flag byte, g7 base, g4/g5 sentinels, fresh frame. */
static void prime_leg(vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint8_t flag)
{
    put_u8(machine, FLAG_ADDR, flag);
    put_u32(machine, WORD_ADDR, TEST_WORD);
    cpu->ip = ENTRY_IP;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = G7_BASE;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = G4_SENTINEL;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = G5_SENTINEL;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
}

static uint32_t reg_g4(const vf2_i960_cpu *cpu) {
    return cpu->registers[VF2_I960_G0_REGISTER + 4u];
}
static uint32_t reg_g5(const vf2_i960_cpu *cpu) {
    return cpu->registers[VF2_I960_G0_REGISTER + 5u];
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ----- Leg A1: flag = 0x01 -> admitted, no persistent write ----- */
    {
        uint64_t before;
        vf2_status status;
        prime_leg(&machine, &cpu, UINT8_C(0x01));
        before = cpu.executed_instructions;
        status = vf2_hybrid_rob_kage_disp_outside_61b88_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        CHECK(reg_g4(&cpu) == G4_SENTINEL);
        CHECK(reg_g5(&cpu) == G5_SENTINEL);
        CHECK(cpu.executed_instructions - before == UINT64_C(2));
        printf("  Leg A1: flag=0x01 -> OK ip=0x61ba0 g4/g5 kept +2\n");
    }

    /* ----- Leg A2: flag = 0xff -> admitted (gate is !=0, not ==1) ----- */
    {
        uint64_t before;
        vf2_status status;
        prime_leg(&machine, &cpu, UINT8_C(0xff));
        before = cpu.executed_instructions;
        status = vf2_hybrid_rob_kage_disp_outside_61b88_execute(
            &machine, &cpu);
        CHECK(status == VF2_OK);
        CHECK(cpu.ip == POST_RET_IP);
        CHECK(reg_g4(&cpu) == G4_SENTINEL);
        CHECK(reg_g5(&cpu) == G5_SENTINEL);
        CHECK(cpu.executed_instructions - before == UINT64_C(2));
        printf("  Leg A2: flag=0xff -> OK ip=0x61ba0 g4/g5 kept +2\n");
    }

    /* ----- Leg B: flag = 0x00 -> refused at own ret slot ----- */
    {
        uint64_t before;
        vf2_status status;
        prime_leg(&machine, &cpu, UINT8_C(0x00));
        before = cpu.executed_instructions;
        status = vf2_hybrid_rob_kage_disp_outside_61b88_execute(
            &machine, &cpu);
        CHECK(status == VF2_ERROR_UNSUPPORTED);
        CHECK(cpu.ip == REFUSE_IP);
        CHECK(reg_g4(&cpu) == TEST_WORD);
        CHECK(reg_g5(&cpu) == UINT32_C(2));
        CHECK(cpu.executed_instructions - before == UINT64_C(4));
        /* The fixture word itself is undisturbed (ld, not st). */
        CHECK(get_u32(&machine, WORD_ADDR) == TEST_WORD);
        printf("  Leg B: flag=0x00 -> REFUSED ip=0x61b9c g4=word g5=2 +4\n");
    }

    /* ----- Leg C: entry guard (wrong ip refuses, no side effects) ----- */
    {
        uint64_t before;
        vf2_status status;
        prime_leg(&machine, &cpu, UINT8_C(0x00));
        cpu.ip = UINT32_C(0x00061b90);
        before = cpu.executed_instructions;
        status = vf2_hybrid_rob_kage_disp_outside_61b88_execute(
            &machine, &cpu);
        CHECK(status == VF2_ERROR_UNSUPPORTED);
        CHECK(reg_g4(&cpu) == G4_SENTINEL);
        CHECK(reg_g5(&cpu) == G5_SENTINEL);
        CHECK(cpu.executed_instructions - before == UINT64_C(0));
        printf("  Leg C: ip=0x61b90 -> REFUSED no side effects +0\n");
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x61b88 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x61b88 rob_kage_disp_outside (4 legs)\n");
    return 0;
}
