/* ====================================================================
 * 0x5ff7c block_hash_apply ctest entry (v0772)
 *
 * Exercises the recovered thin wrapper
 * vf2_hybrid_block_hash_apply_5ff7c_execute which:
 *   1. reads a 32-bit base pointer from 0x50016c (work_ram)
 *   2. sets g0 = base + 0x33a8 (input buffer)
 *   3. sets g1 = 0, g2 = 0x644
 *   4. calls the recovered 0x9480 (CRC16 step hash)
 *   5. stos the 32-bit result to 0x01d03304 (backup_sram)
 *   6. ret -> 0x5ffa4
 *
 * Per vf2i960 disasm (40 B, 1 block):
 *   0005ff7c  ld       0x0050016c, r15
 *   0005ff84  lda      0x000033a8(r15), g0
 *   0005ff8c  lda      0x00000644, g2
 *   0005ff90  mov      0, g1
 *   0005ff94  call     0x00009480
 *   0005ff98  stos     g0, 0x01d03304
 *   0005ffa0  ret
 *
 * Test legs (all 4 set the same pointer at 0x50016c; differ in
 * the input pattern and the table used by the 0x9480 sub-call):
 *   A: identity bytes (0,1,2,...,1602), default table
 *   B: 0xaa55 repeating (1602 bytes), default table
 *   C: 0x10..0x00 stepping (1602 bytes), linear table
 *   D: all-0xff bytes, linear table (worst-case high bytes)
 *
 * For each leg, the test asserts:
 *   - status == VF2_OK
 *   - cpu->ip == POST_RET_IP (0x5ffa4)
 *   - the value at 0x01d03304 == naive(input, table)
 *   where naive() is a 1-to-1 C implementation of the
 *   0x9480 hash on the same input + table.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x0005ff7c)
#define POST_RET_IP UINT32_C(0x0005ffa4)
#define TABLE_BASE  UINT32_C(0x02000000)
/* INPUT_BASE must be at least 0x644 bytes PAST PTR_ADDR
 * (0x50016c) so the input write doesn't trample the
 * pointer that the 0x5ff7c recovery reads. 0x500800 is
 * 0x694 bytes past PTR_ADDR, well outside the input range. */
#define INPUT_BASE  UINT32_C(0x00500800)
#define INPUT_COUNT UINT32_C(0x644)  /* 1604: 1603 iters + 1 sentinel */
#define PTR_ADDR    UINT32_C(0x0050016c)
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
    /* Default: high byte = ~idx, low byte = idx. */
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        table[i] = (uint16_t)(((~(uint32_t)i & 0xffu) << 8) |
                                (i & 0xffu));
    }
}

static void fill_table_linear(uint16_t *table) {
    /* Deterministic linear: idx*7 + 0x1234. */
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        table[i] = (uint16_t)((i * 7u + 0x1234u) & 0xffffu);
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

/* Direct 1-to-1 C implementation of the 0x9480 hash.
 * n = number of input bytes (= 0x643 = 1603 for this caller).
 * stride = 1 + g1 (g1 = 0 here, so stride = 1). */
static uint16_t naive_hash(const uint8_t *input, size_t n,
                           const uint16_t *table) {
    uint32_t r3 = 0u;
    int32_t r9 = (int32_t)(n + 1u) - 1;
    size_t i;
    for (i = 0u; i < n; ++i) {
        uint32_t r5;
        uint8_t r3_byte;
        uint16_t table_val;
        /* shlo 8, r3, r5; and 0xff00, r5, r5 */
        r5 = (r3 << 8) & 0xff00u;
        /* shro 8, r3, r3 */
        r3 = r3 >> 8;
        /* ldob input[i], r6 */
        r3 = (r3 ^ (uint32_t)input[i]) & 0xffu;
        /* table[r3] */
        r3_byte = (uint8_t)r3;
        table_val = table[r3_byte];
        r3 = (uint32_t)table_val;
        /* xor r5, r3, r3 */
        r3 = r3 ^ r5;
        /* cmpdeci 0, r9, r9: r9-1; branch if r9 > 0 (signed) */
        r9 -= 1;
        if (r9 <= 0) {
            break;
        }
    }
    return (uint16_t)r3;
}

static uint32_t run_recovery(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    vf2_status status;
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_block_hash_apply_5ff7c_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);
    return get_u32(machine, RESULT_ADDR);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    /* main_data backing for the 0x9480 table (read-only at the
     * model2a boundary; pre-fill in C and attach). */
    static uint8_t main_data_buf[0x10000];
    /* Local copy of the input for the naive impl. */
    static uint8_t input[INPUT_COUNT];

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    CHECK(vf2_model2a_attach_main_data(
        &machine, main_data_buf, sizeof(main_data_buf)) == VF2_OK);
    /* Set the base pointer at 0x50016c so that base + 0x33a8
     * lands inside work_ram at INPUT_BASE. work_ram is
     * auto-allocated (1 MB starting at 0x00500000), so the
     * input at INPUT_BASE is valid. INPUT_BASE is at offset
     * >= 0x800 so the input write (0x644 bytes) does not
     * trample the pointer at 0x50016c (offset 0x16c). */
    put_u32(&machine, PTR_ADDR, INPUT_BASE - UINT32_C(0x33a8));
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ----- Leg A: identity bytes, default table ----- */
    {
        uint16_t table[256];
        uint32_t i;
        for (i = 0u; i < INPUT_COUNT - 1u; ++i) {
            input[i] = (uint8_t)(i & 0xffu);
        }
        input[INPUT_COUNT - 1u] = 0u;  /* sentinel */
        fill_table_default(table);
        upload_table(main_data_buf, table);
        /* Write the input bytes to work_ram at INPUT_BASE. */
        for (i = 0u; i < INPUT_COUNT; ++i) {
            put_u8(&machine, INPUT_BASE + i, input[i]);
        }
        uint32_t recovered = run_recovery(&machine, &cpu);
        uint16_t naive = naive_hash(input, INPUT_COUNT - 1u, table);
        CHECK((uint16_t)recovered == naive);
        printf("  Leg A: identity bytes 0..1602, default table "
               "-> 0x%08x\n", recovered);
    }

    /* ----- Leg B: 0xaa55 repeating, default table ----- */
    {
        uint16_t table[256];
        uint32_t i;
        for (i = 0u; i < INPUT_COUNT - 1u; ++i) {
            input[i] = (i & 1u) ? 0x55u : 0xaau;
        }
        input[INPUT_COUNT - 1u] = 0u;
        fill_table_default(table);
        upload_table(main_data_buf, table);
        for (i = 0u; i < INPUT_COUNT; ++i) {
            put_u8(&machine, INPUT_BASE + i, input[i]);
        }
        uint32_t recovered = run_recovery(&machine, &cpu);
        uint16_t naive = naive_hash(input, INPUT_COUNT - 1u, table);
        CHECK((uint16_t)recovered == naive);
        printf("  Leg B: 0xaa55 repeating (1602 B), default table "
               "-> 0x%08x\n", recovered);
    }

    /* ----- Leg C: 0x10..0x00 stepping, linear table ----- */
    {
        uint16_t table[256];
        uint32_t i;
        for (i = 0u; i < INPUT_COUNT - 1u; ++i) {
            input[i] = (uint8_t)((0x10u - (i & 0xffu)) & 0xffu);
        }
        input[INPUT_COUNT - 1u] = 0u;
        fill_table_linear(table);
        upload_table(main_data_buf, table);
        for (i = 0u; i < INPUT_COUNT; ++i) {
            put_u8(&machine, INPUT_BASE + i, input[i]);
        }
        uint32_t recovered = run_recovery(&machine, &cpu);
        uint16_t naive = naive_hash(input, INPUT_COUNT - 1u, table);
        CHECK((uint16_t)recovered == naive);
        printf("  Leg C: stepping 0x10..0x00 (1602 B), linear table "
               "-> 0x%08x\n", recovered);
    }

    /* ----- Leg D: all-0xff, linear table ----- */
    {
        uint16_t table[256];
        uint32_t i;
        for (i = 0u; i < INPUT_COUNT - 1u; ++i) {
            input[i] = 0xffu;
        }
        input[INPUT_COUNT - 1u] = 0u;
        fill_table_linear(table);
        upload_table(main_data_buf, table);
        for (i = 0u; i < INPUT_COUNT; ++i) {
            put_u8(&machine, INPUT_BASE + i, input[i]);
        }
        uint32_t recovered = run_recovery(&machine, &cpu);
        uint16_t naive = naive_hash(input, INPUT_COUNT - 1u, table);
        CHECK((uint16_t)recovered == naive);
        printf("  Leg D: all-0xff (1602 B), linear table "
               "-> 0x%08x\n", recovered);
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x5ff7c (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x5ff7c block_hash_apply (4 legs)\n");
    return 0;
}
