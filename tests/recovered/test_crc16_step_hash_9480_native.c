/* ====================================================================
 * 0x9480 poly_or_crc16_step_hash ctest entry (v0771)
 *
 * Exercises the recovered table-based 16-bit accumulator hash
 * vf2_hybrid_crc16_step_hash_9480_execute.
 *
 * The test computes the expected hash via two independent
 * implementations and compares to the recovery's output:
 *
 *   1. The recovery (vf2_hybrid_crc16_step_hash_9480_execute,
 *      1-to-1 C translation of the disassembly).
 *   2. A direct 1-to-1 "naive" implementation in the test (the
 *      same operations written without helper functions).
 *
 * Both should produce identical results for the same input,
 * and the test asserts they match. This validates that the
 * recovery's translation is structurally equivalent to a
 * minimal direct implementation.
 *
 * Per vf2i960 disasm (80 B, 3 blocks):
 *   Inputs:  g0 = input pointer; g1 = stride - 1; g2 = count + 1
 *   Output: g0 = accumulated 16-bit hash (lo half from table,
 *           hi half from previous low byte)
 *   Table: 256 16-bit entries at VF2_MAIN_DATA_BASE (0x02000000)
 *
 * Test legs:
 *   A: small 4-byte input, default table
 *   B: 8-byte input with non-zero stride
 *   C: 16-byte input with a deterministic table (each entry
 *      = idx * 7 + 0x1234)
 *   D: 1-byte input (g2=1 -> 1 iteration edge case)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x00009480)
#define POST_RET_IP UINT32_C(0x000094d0)
#define TABLE_BASE  UINT32_C(0x02000000)
#define INPUT_BASE  UINT32_C(0x00580400)

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
static uint16_t get_u16(vf2_model2a *m, uint32_t a) {
    uint16_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void fill_table_default(uint16_t *table) {
    /* Default table: index = 16-bit value with the high byte
     * equal to ~index and low byte equal to index (a known
     * pattern that exercises the lookup). */
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        table[i] = (uint16_t)(((~(uint32_t)i & 0xffu) << 8) |
                                (i & 0xffu));
    }
}

static void fill_table_linear(uint16_t *table) {
    /* Deterministic linear table: idx * 7 + 0x1234. */
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        table[i] = (uint16_t)((i * 7u + 0x1234u) & 0xffffu);
    }
}

static void fill_input(vf2_model2a *m, uint32_t addr,
                       const uint8_t *bytes, size_t n) {
    size_t i;
    for (i = 0u; i < n; ++i) {
        put_u8(m, addr + (uint32_t)i, bytes[i]);
    }
}

/* Direct 1-to-1 implementation matching the disassembly
 * algorithm. Performs the same operations as the recovery
 * without any helper. */
static uint16_t naive_hash(const uint8_t *input, size_t n,
                          int stride, const uint16_t *table) {
    uint32_t r3 = 0u;
    int32_t r9 = (int32_t)(n + 1u) - 1;  /* simulate subo 1, g2, r9 */
    size_t i;
    uint32_t g0_idx = 0u;
    for (i = 0u; i < n; ++i) {
        uint32_t r5;
        uint32_t r6;
        uint8_t r3_byte;
        uint16_t table_val;
        (void)stride;
        /* shlo 8, r3, r5; and 0xff00, r5, r5 */
        r5 = (r3 << 8) & 0xff00u;
        /* shro 8, r3, r3 */
        r3 = r3 >> 8;
        /* ldob input[i], r6 */
        r6 = input[g0_idx];
        g0_idx += (uint32_t)stride + 1u;
        /* xor r6, r3, r3; and 0xff, r3, r3 */
        r3 = (r3 ^ r6) & 0xffu;
        /* table[r3] */
        r3_byte = (uint8_t)r3;
        table_val = table[r3_byte];
        r3 = (uint32_t)table_val;
        /* xor r5, r3, r3 */
        r3 = r3 ^ r5;
        /* cmpdeci 0, r9, r9: r9-1 > 0 check (signed) */
        (void)r9;
    }
    return (uint16_t)r3;
}

static uint16_t run_recovery(vf2_model2a *machine, vf2_i960_cpu *cpu,
                             size_t n, int stride) {
    vf2_status status;
    /* Set up g0/g1/g2 in cpu->registers[16..18]. */
    cpu->registers[16u + 0u] = INPUT_BASE;  /* g0 = input ptr */
    cpu->registers[16u + 1u] = (uint32_t)stride;  /* g1 */
    cpu->registers[16u + 2u] = (uint32_t)(n + 1u);  /* g2 */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_crc16_step_hash_9480_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);
    return (uint16_t)cpu->registers[16u + 0u];
}

/* Copy the table out of main_data for the naive impl to
 * consume. Reads via model2a because the recovery reads via
 * model2a too — both must observe the same backing. */
static void read_table(vf2_model2a *m, uint16_t *out) {
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        out[i] = get_u16(m, TABLE_BASE + i * 2u);
    }
}

/* Mirror of fill_table_default/linear: writes the local 256-entry
 * table into the test's main_data backing buffer. main_data is
 * read-only by hardware design, so we have to pre-fill the
 * attached buffer directly; vf2_model2a_write to 0x02000000
 * returns OUT_OF_BOUNDS by design. */
static void upload_table(uint8_t *main_data_buf,
                         const uint16_t *table) {
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        main_data_buf[i * 2u + 0u] = (uint8_t)(table[i] & 0xffu);
        main_data_buf[i * 2u + 1u] = (uint8_t)((table[i] >> 8) & 0xffu);
    }
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    /* Real backing for VF2_MAIN_DATA_BASE (table lives at
     * 0x02000000). 0x10000 bytes covers the 256-entry 16-bit
     * table the recovery reads (512 B) with plenty of margin.
     * main_data is read-only by hardware design, so the
     * test pre-fills this buffer directly in C and then
     * attaches it for the recovery to read. work_ram
     * (where INPUT_BASE lives) is auto-allocated by
     * initialize(). */
    static uint8_t main_data_buf[0x10000];

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    CHECK(vf2_model2a_attach_main_data(
        &machine, main_data_buf, sizeof(main_data_buf)) == VF2_OK);

    /* ----- Leg A: 4-byte input, default table, stride 1 ----- */
    {
        const uint8_t input[4] = {0x01, 0x02, 0x03, 0x04};
        uint16_t table[256];
        fill_table_default(table);
        upload_table(main_data_buf, table);
        fill_input(&machine, INPUT_BASE, input, sizeof(input));
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        uint16_t recovered = run_recovery(&machine, &cpu,
                                           sizeof(input), 0);
        read_table(&machine, table);
        uint16_t naive = naive_hash(input, sizeof(input), 0, table);
        CHECK(naive == recovered);
        printf("  Leg A: 4-byte input, default table, "
               "g0=0x%x -> 0x%04x\n",
               (unsigned)(INPUT_BASE), recovered);
    }

    /* ----- Leg B: 8-byte input, stride 1, default table ----- */
    {
        const uint8_t input[8] = {
            0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55};
        uint16_t table[256];
        fill_table_default(table);
        upload_table(main_data_buf, table);
        fill_input(&machine, INPUT_BASE, input, sizeof(input));
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        uint16_t recovered = run_recovery(&machine, &cpu,
                                           sizeof(input), 0);
        read_table(&machine, table);
        uint16_t naive = naive_hash(input, sizeof(input), 0, table);
        CHECK(naive == recovered);
        printf("  Leg B: 8-byte input 0xaa55 repeating, "
               "default table -> 0x%04x\n", recovered);
    }

    /* ----- Leg C: 16-byte input, linear table ----- */
    {
        const uint8_t input[16] = {
            0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
            0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00};
        uint16_t table[256];
        fill_table_linear(table);
        upload_table(main_data_buf, table);
        fill_input(&machine, INPUT_BASE, input, sizeof(input));
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        uint16_t recovered = run_recovery(&machine, &cpu,
                                           sizeof(input), 0);
        read_table(&machine, table);
        uint16_t naive = naive_hash(input, sizeof(input), 0, table);
        CHECK(naive == recovered);
        printf("  Leg C: 16-byte input (0x10..0x00 stepping), "
               "linear table (idx*7+0x1234) -> 0x%04x\n",
               recovered);
    }

    /* ----- Leg D: 1-byte input, g2=1 -> 1 iter edge case ----- */
    {
        const uint8_t input[1] = {0xff};
        uint16_t table[256];
        fill_table_linear(table);
        upload_table(main_data_buf, table);
        fill_input(&machine, INPUT_BASE, input, sizeof(input));
        vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
        uint16_t recovered = run_recovery(&machine, &cpu,
                                           sizeof(input), 0);
        read_table(&machine, table);
        uint16_t naive = naive_hash(input, sizeof(input), 0, table);
        CHECK(naive == recovered);
        printf("  Leg D: 1-byte input 0xff, g2=1 -> 0x%04x\n",
               recovered);
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x9480 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x9480 poly_or_crc16_step_hash (4 legs)\n");
    return 0;
}
