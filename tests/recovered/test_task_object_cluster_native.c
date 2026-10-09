/* ====================================================================
 * task_object 0x6ca64 + 5-cluster ctest entry (v0767)
 *
 * Exercises the recovered task_object dispatcher and its 5 init
 * siblings (init0, init0_ret, init1, init1_ret, init2). The
 * dispatcher reads g13+0x4 byte, looks up jump table at 0x6ca78,
 * and stores the looked-up value to g13+0xc. Each init sets
 * g13+0xc to its post-ret slot and "returns".
 *
 * The jump table at 0x6ca78 is in ROM space (Model 2A program
 * data). The test attaches a fake main_rom covering the source
 * read at 0x6ca78.
 *
 * Per vf2i960 disasm, the 5 entries are:
 *   task_object           0x6ca64..0x6ca78   (20 B, 1 block) - dispatcher
 *   task_object_init0     0x6cae0..0x6caf0   (16 B, 1 block)
 *   task_object_init0_ret 0x6caf0..0x6caf4   ( 4 B, 1 block, bare ret)
 *   task_object_init1     0x6caf4..0x6cb04   (16 B, 1 block)
 *   task_object_init1_ret 0x6cb04..0x6cb08   ( 4 B, 1 block, bare ret)
 *   task_object_init2     0x6cb08..0x6cb0c   ( 4 B, 1 block, bare ret)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define G13_REGISTER (16u + 13u)

/* IPs (entry -> post-ret or next-IP). */
#define TASK_OBJECT_IP    UINT32_C(0x0006ca64)
#define TASK_OBJECT_POST  UINT32_C(0x0006ca78) /* next instruction past ret */
#define INIT0_IP          UINT32_C(0x0006cae0)
#define INIT0_POST        UINT32_C(0x0006caf0) /* init0_ret slot */
#define INIT0_RET_IP      UINT32_C(0x0006caf0)
#define INIT0_RET_POST    UINT32_C(0x0006caf4) /* one past bare ret */
#define INIT1_IP          UINT32_C(0x0006caf4)
#define INIT1_POST        UINT32_C(0x0006cb04)
#define INIT1_RET_IP      UINT32_C(0x0006cb04)
#define INIT1_RET_POST    UINT32_C(0x0006cb08)
#define INIT2_IP          UINT32_C(0x0006cb08)
#define INIT2_POST        UINT32_C(0x0006cb0c)
#define G13_BASE          UINT32_C(0x00580800)
#define JUMP_TABLE_BASE   UINT32_C(0x6ca78)
#define ROM_SIZE          0x80000u

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

static void put_u32_wr(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, a, v) == VF2_OK);
}
static void put_u8_wr(vf2_model2a *m, uint32_t a, uint8_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(m, a, &v) == VF2_OK);
    return v;
}

/* Lay down the 3-entry jump table at 0x6ca78 inside the fake
 * program ROM. The table has 4-byte entries; index i reads from
 * address 0x6ca78 + i*4. */
static void lay_jump_table(uint8_t *rom, uint32_t base,
                           uint32_t init0, uint32_t init1,
                           uint32_t init2) {
    rom[base + 0u * 4u + 0u] = (uint8_t)(init0 & 0xffu);
    rom[base + 0u * 4u + 1u] = (uint8_t)((init0 >> 8) & 0xffu);
    rom[base + 0u * 4u + 2u] = (uint8_t)((init0 >> 16) & 0xffu);
    rom[base + 0u * 4u + 3u] = (uint8_t)((init0 >> 24) & 0xffu);
    rom[base + 1u * 4u + 0u] = (uint8_t)(init1 & 0xffu);
    rom[base + 1u * 4u + 1u] = (uint8_t)((init1 >> 8) & 0xffu);
    rom[base + 1u * 4u + 2u] = (uint8_t)((init1 >> 16) & 0xffu);
    rom[base + 1u * 4u + 3u] = (uint8_t)((init1 >> 24) & 0xffu);
    rom[base + 2u * 4u + 0u] = (uint8_t)(init2 & 0xffu);
    rom[base + 2u * 4u + 1u] = (uint8_t)((init2 >> 8) & 0xffu);
    rom[base + 2u * 4u + 2u] = (uint8_t)((init2 >> 16) & 0xffu);
    rom[base + 2u * 4u + 3u] = (uint8_t)((init2 >> 24) & 0xffu);
}

static void prime_worker_record(vf2_model2a *m, uint8_t instance_index,
                                uint32_t pre_next) {
    put_u8_wr(m, G13_BASE + 4u, instance_index);
    put_u32_wr(m, G13_BASE + 0xc, pre_next);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;

    static uint8_t fake_rom[ROM_SIZE];
    memset(fake_rom, 0, ROM_SIZE);
    lay_jump_table(fake_rom, JUMP_TABLE_BASE, INIT0_IP, INIT1_IP, INIT2_IP);

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    CHECK(vf2_model2a_attach_main_rom(
        &machine, fake_rom, sizeof(fake_rom)) == VF2_OK);

    /* ----- Leg A: dispatcher index=0 -> init0 ----- */
    prime_worker_record(&machine, 0u, 0xdeadbeefu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, TASK_OBJECT_IP);
    cpu.registers[G13_REGISTER] = G13_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, TASK_OBJECT_IP, TASK_OBJECT_POST) == VF2_OK);
    status = vf2_hybrid_task_object_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == TASK_OBJECT_POST);
    CHECK(get_u32(&machine, G13_BASE + 0xc) == INIT0_IP);

    /* ----- Leg B: dispatcher index=1 -> init1 ----- */
    prime_worker_record(&machine, 1u, 0xdeadbeefu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, TASK_OBJECT_IP);
    cpu.registers[G13_REGISTER] = G13_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, TASK_OBJECT_IP, TASK_OBJECT_POST) == VF2_OK);
    status = vf2_hybrid_task_object_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == TASK_OBJECT_POST);
    CHECK(get_u32(&machine, G13_BASE + 0xc) == INIT1_IP);

    /* ----- Leg C: dispatcher index=2 -> init2 ----- */
    prime_worker_record(&machine, 2u, 0xdeadbeefu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, TASK_OBJECT_IP);
    cpu.registers[G13_REGISTER] = G13_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, TASK_OBJECT_IP, TASK_OBJECT_POST) == VF2_OK);
    status = vf2_hybrid_task_object_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == TASK_OBJECT_POST);
    CHECK(get_u32(&machine, G13_BASE + 0xc) == INIT2_IP);

    /* ----- Leg D: init0 -> set g13+0xc to init0_ret slot ----- */
    put_u32_wr(&machine, G13_BASE + 0xc, 0xdeadbeefu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, INIT0_IP);
    cpu.registers[G13_REGISTER] = G13_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, INIT0_IP, INIT0_POST) == VF2_OK);
    status = vf2_hybrid_task_object_init0_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == INIT0_POST);
    CHECK(get_u32(&machine, G13_BASE + 0xc) == INIT0_RET_IP);

    /* ----- Leg E: init1 -> set g13+0xc to init1_ret slot ----- */
    put_u32_wr(&machine, G13_BASE + 0xc, 0xdeadbeefu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, INIT1_IP);
    cpu.registers[G13_REGISTER] = G13_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, INIT1_IP, INIT1_POST) == VF2_OK);
    status = vf2_hybrid_task_object_init1_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == INIT1_POST);
    CHECK(get_u32(&machine, G13_BASE + 0xc) == INIT1_RET_IP);

    /* ----- Leg F: init0_ret bare ret ----- */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, INIT0_RET_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, INIT0_RET_IP, INIT0_RET_POST) == VF2_OK);
    status = vf2_hybrid_task_object_init0_ret_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == INIT0_RET_POST);

    /* ----- Leg G: init1_ret bare ret ----- */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, INIT1_RET_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, INIT1_RET_IP, INIT1_RET_POST) == VF2_OK);
    status = vf2_hybrid_task_object_init1_ret_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == INIT1_RET_POST);

    /* ----- Leg H: init2 bare ret ----- */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, INIT2_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, INIT2_IP, INIT2_POST) == VF2_OK);
    status = vf2_hybrid_task_object_init2_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == INIT2_POST);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: task_object cluster (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: task_object cluster (8 legs across 5 entries)\n");
    return 0;
}
