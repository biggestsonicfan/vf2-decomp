/* ====================================================================
 * 0x6dccc Model 2A register-cluster switch ctest entry (v0765)
 *
 * Exercises the recovered poly_or_model2a_register_switch
 * vf2_hybrid_model2a_register_switch_6dccc_execute.
 *
 * Per vf2i960 disasm (88 B, 4 blocks):
 *  - read byte from 0x50002b;
 *  - if byte == 16 or byte == 17, return immediately;
 *  - otherwise read u32 from 0x59c318;
 *  - increment by 1;
 *  - mirror to 0x1d03318 (extended bus) and 0x59c318;
 *  - mirror the original byte to 0x1d0331c and 0x59c31c;
 *  - ret.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0006dccc)
#define POST_RET_IP UINT32_C(0x0006dd24)
#define BYTE_AT     UINT32_C(0x0050002b)
#define U32_HI      UINT32_C(0x0059c318)
#define U32_LO      UINT32_C(0x0059c31c)
#define U32_HI_X    UINT32_C(0x01d03318)
#define U32_LO_X    UINT32_C(0x01d0331c)

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

static void put_u8(vf2_model2a *m, uint32_t a, uint8_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, a, v) == VF2_OK);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(m, a, &v) == VF2_OK);
    return v;
}
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void reset_touched(vf2_model2a *m) {
    put_u8(m, BYTE_AT, 0u);
    put_u32(m, U32_HI,  0u);
    put_u32(m, U32_LO,  0u);
    put_u32(m, U32_HI_X, 0u);
    put_u32(m, U32_LO_X, 0u);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;

    if (vf2_model2a_initialize(&machine) == 0) return 1;

    /* ----- Leg A: byte == 16, ret without writes ----- */
    reset_touched(&machine);
    put_u8(&machine, BYTE_AT, 16u);
    put_u32(&machine, U32_HI, 0x12345678u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_model2a_register_switch_6dccc_execute(
        &machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == POST_RET_IP);
    CHECK(get_u32(&machine, U32_HI)   == 0x12345678u);
    CHECK(get_u32(&machine, U32_HI_X) == 0u);
    CHECK(get_u32(&machine, U32_LO_X) == 0u);

    /* ----- Leg B: byte == 17, ret without writes ----- */
    reset_touched(&machine);
    put_u8(&machine, BYTE_AT, 17u);
    put_u32(&machine, U32_HI, 0xaabbccddu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_model2a_register_switch_6dccc_execute(
        &machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == POST_RET_IP);
    CHECK(get_u32(&machine, U32_HI)   == 0xaabbccddu);
    CHECK(get_u32(&machine, U32_HI_X) == 0u);

    /* ----- Leg C: byte == 5 (no sentinel), increment path ----- */
    reset_touched(&machine);
    put_u8(&machine, BYTE_AT, 5u);
    put_u32(&machine, U32_HI, 0x00000041u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_model2a_register_switch_6dccc_execute(
        &machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == POST_RET_IP);
    /* r14 was incremented by 1 -> 0x00000042. */
    CHECK(get_u32(&machine, U32_HI)   == 0x00000042u);
    CHECK(get_u32(&machine, U32_HI_X) == 0x00000042u);
    /* r15 (the byte, 5) was mirrored unmodified. */
    CHECK(get_u32(&machine, U32_LO_X) == 5u);
    CHECK(get_u32(&machine, U32_LO)   == 5u);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x6dccc (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x6dccc poly_or_model2a_register_switch (3 legs)\n");
    return 0;
}
