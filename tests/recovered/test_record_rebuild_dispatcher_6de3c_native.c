/* ====================================================================
 * 0x6de3c record-rebuild dispatcher ctest entry (v0782)
 *
 * Exercises the recovered dispatcher
 * vf2_hybrid_record_rebuild_dispatcher_6de3c_execute which:
 *   1. Calls 0x9480 (v0771 hash) with caller-supplied g0/g1/g2
 *      -> g0 = hash1. Compare to stored at 0x01d03302.
 *   2. If different: rebuild via 0x6010c + 0x5427c, recompute
 *      hash, store at 0x01d03302.
 *   3. Compute hash of bus+0x3320 15 bytes via 0x9480.
 *   4. Compare to stored at 0x01d03300. If different: clear
 *      via 0x60330 + 0x5427c, recompute, store at 0x01d03300.
 *   5. Set bus_ptr = 0x01d00000, set bit 0 of 0x500171.
 *   6. Mirror copy 0x3fe0 bytes from 0x1d00000 to 0x599000.
 *   7. Write "VIRTUA FIGHTER 2" signature at bus+0x3306..17.
 *   8. Set bus_ptr = 0x00599000, zero 0x500148..0x50014b.
 *
 * Per vf2i960 disasm (912 B, 9 blocks, 56 instr).
 *
 * Legs:
 *   A: stored hashes match (no rebuild) -> only mirror copy +
 *      signature write + reset.
 *   B: stored hash 0x3302 differs -> assignment block rebuilt,
 *      mirror copy + signature + reset.
 *   C: stored hash 0x3300 differs -> poly record cleared and
 *      reloaded, mirror copy + signature + reset.
 *   D: both hashes differ -> both rebuilds, mirror copy +
 *      signature + reset.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP     UINT32_C(0x0006de3c)
#define POST_RET_IP  UINT32_C(0x0006e1cc)
#define BUS_PTR_ADDR UINT32_C(0x0050016c)
#define STORED_H1    UINT32_C(0x01d03302)
#define STORED_H2    UINT32_C(0x01d03300)
#define FLAG_ADDR    UINT32_C(0x00500171)
#define ZERO_BASE    UINT32_C(0x00500148)
#define SRC_BASE     UINT32_C(0x01d00000)
#define DST_BASE     UINT32_C(0x00599000)
#define COPY_SIZE    0x3fe0u
#define MAIN_DATA_SIZE 0x20000u
/* v0776 poly_record_load sub-call inputs. */
#define POLY_TABLE_BASE UINT32_C(0x0201eafc)
#define POLY_INPUT_BASE UINT32_C(0x0201ea24)
#define POLY_DST_BASE   UINT32_C(0x01d03100)
#define POLY_TABLE_OFFSET 0x1eafcu
#define POLY_INPUT_OFFSET 0x1ea24u
#define POLY_TABLE_WORDS  18u
#define POLY_INPUT_LEN    (18u * 12u)
/* v0775 timer_fuse sub-call inputs. */
#define TIMER_BASE UINT32_C(0x00f00000)
#define FUSE_ADDR  UINT32_C(0x00500098)
/* Polygon pointer target (work_ram). */
#define POLY_DATA_TARGET UINT32_C(0x005a0000)

static uint8_t main_data[MAIN_DATA_SIZE];

static void fill_table_default(uint16_t *table) {
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        table[i] = (uint16_t)(((~(uint32_t)i & 0xffu) << 8) |
                                (i & 0xffu));
    }
}

static void upload_table(uint16_t *table) {
    uint32_t i;
    for (i = 0u; i < 256u; ++i) {
        main_data[i * 2u + 0u] = (uint8_t)(table[i] & 0xffu);
        main_data[i * 2u + 1u] = (uint8_t)((table[i] >> 8) & 0xffu);
    }
}

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
static void put_u16(vf2_model2a *m, uint32_t a, uint16_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}
static uint16_t get_u16(vf2_model2a *m, uint32_t a) {
    uint16_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

/* Pre-fill the 0x3fe0 byte backup_sram region with a known
 * pattern; the dispatcher copies this to extended_work_ram. */
static void fill_source(vf2_model2a *machine, uint8_t fill) {
    uint32_t i;
    for (i = 0u; i < COPY_SIZE; ++i) {
        put_u8(machine, SRC_BASE + i, (uint8_t)(fill + (uint8_t)i));
    }
}

/* Compute the v0771 hash of `len` bytes at `addr` using the
 * v0771 wrapper directly. We do this to set up the stored
 * hashes so the dispatcher sees a match (no rebuild) or
 * a mismatch (forces rebuild). */
static uint16_t compute_hash(vf2_model2a *machine, vf2_i960_cpu *cpu,
                              uint32_t addr, uint32_t len) {
    vf2_status status;
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = addr;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = len;
    cpu->ip = UINT32_C(0x00009480);
    status = vf2_hybrid_crc16_step_hash_9480_execute(machine, cpu);
    CHECK(status == VF2_OK);
    return (uint16_t)cpu->registers[VF2_I960_G0_REGISTER + 0u];
}

static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    int mismatch_h1,
    int mismatch_h2,
    uint8_t fill)
{
    vf2_status status;
    uint32_t i;
    uint16_t stored_h1, stored_h2;
    uint16_t true_h1, true_h2;

    /* Set up bus_ptr = 0x00500000 so the dispatcher's
     * "bus + 0x3340" and "bus + 0x3320" reads point into
     * work_ram at 0x503340 / 0x503320. We'll fill those
     * with our test data. */
    put_u32(machine, BUS_PTR_ADDR, UINT32_C(0x00500000));

    /* Pre-fill work_ram 0x503320..0x50333e (15 bytes) and
     * 0x503340..0x50335c (29 bytes) with the test fill. */
    for (i = 0u; i < 29u; ++i) {
        put_u8(machine, UINT32_C(0x00503340) + i,
               (uint8_t)(fill + (uint8_t)(i * 7u)));
    }
    for (i = 0u; i < 15u; ++i) {
        put_u8(machine, UINT32_C(0x00503320) + i,
               (uint8_t)(fill + (uint8_t)(i * 11u)));
    }

    /* Compute the true hashes. */
    true_h1 = compute_hash(machine, cpu, UINT32_C(0x00503340), 29u);
    true_h2 = compute_hash(machine, cpu, UINT32_C(0x00503320), 15u);

    /* Set stored hashes. If mismatch, use a value that
     * cannot match the true hash (use ~true_hash). */
    stored_h1 = mismatch_h1 ? (uint16_t)(~true_h1) : true_h1;
    stored_h2 = mismatch_h2 ? (uint16_t)(~true_h2) : true_h2;
    put_u16(machine, STORED_H1, stored_h1);
    put_u16(machine, STORED_H2, stored_h2);

    /* Pre-fill source for mirror copy. */
    fill_source(machine, fill);

    /* v0776 poly_record_load sub-call setup.
     *   - Timers: t0=1, others=0 -> v0775 returns 1, 2, ..., 5
     *     for the 6 alloc-path iters. r10=6..17 use the
     *     prealloc path (slot = r10 directly).
     *   - Polygon_table[0..17] = POLY_DATA_TARGET (work_ram).
     *   - POLY_DATA_TARGET[..] = 0xdeadbeef (so the deref
     *     at *polygon_table reads a valid u32).
     *   - Input data 0x0201ea24: 18 entries * 12 bytes. */
    put_u32(machine, TIMER_BASE + 0u, 1u);
    put_u32(machine, TIMER_BASE + 4u, 0u);
    put_u32(machine, TIMER_BASE + 8u, 0u);
    put_u32(machine, TIMER_BASE + 12u, 0u);
    put_u32(machine, FUSE_ADDR, 0u);  /* pre-fuse = 0 */
    /* Polygon_table: 18 entries, each = POLY_DATA_TARGET. */
    for (i = 0u; i < POLY_TABLE_WORDS; ++i) {
        main_data[POLY_TABLE_OFFSET + i * 4u + 0u] =
            (uint8_t)((POLY_DATA_TARGET >> 0) & 0xffu);
        main_data[POLY_TABLE_OFFSET + i * 4u + 1u] =
            (uint8_t)((POLY_DATA_TARGET >> 8) & 0xffu);
        main_data[POLY_TABLE_OFFSET + i * 4u + 2u] =
            (uint8_t)((POLY_DATA_TARGET >> 16) & 0xffu);
        main_data[POLY_TABLE_OFFSET + i * 4u + 3u] =
            (uint8_t)((POLY_DATA_TARGET >> 24) & 0xffu);
    }
    /* POLY_DATA_TARGET contents (deref reads here). */
    put_u32(machine, POLY_DATA_TARGET, 0xdeadbeefu);
    /* Input data 0x0201ea24: 18 entries * 12 bytes. */
    for (i = 0u; i < POLY_INPUT_LEN; ++i) {
        main_data[POLY_INPUT_OFFSET + i] = (uint8_t)(fill + (uint8_t)i);
    }
    /* Pre-fill v0778 snapshot region with sentinel. */
    for (i = 0u; i < 7u; ++i) {
        put_u8(machine, UINT32_C(0x00500234) + i, 0xaau);
    }

    /* Pre-fill destination sentinel. */
    for (i = 0u; i < COPY_SIZE; ++i) {
        put_u8(machine, DST_BASE + i, 0xaau);
    }
    /* Pre-fill flag byte 0x500171 with 0. */
    put_u8(machine, FLAG_ADDR, 0u);
    /* Pre-fill zero region 0x500148..0x50014b with 0xff. */
    for (i = 0u; i < 4u; ++i) {
        put_u8(machine, ZERO_BASE + i, 0xffu);
    }
    /* Clear the assignment block in backup_sram so we can
     * detect if 0x6010c was called. */
    for (i = 0u; i < 29u; ++i) {
        put_u8(machine, UINT32_C(0x01d03340) + i, 0xeeu);
    }
    /* Clear the poly record in backup_sram. */
    for (i = 0u; i < 15u; ++i) {
        put_u8(machine, UINT32_C(0x01d03320) + i, 0xeeu);
    }
    /* Pre-fill v0778 snapshot region with sentinel. */
    for (i = 0u; i < 7u; ++i) {
        put_u8(machine, UINT32_C(0x00500234) + i, 0xaau);
    }
    /* Set bus_ptr back so the dispatcher's initial
     * 0x9480 call (which uses caller-supplied g0/g1/g2)
     * points to a valid region. The 0x9480 hash result
     * is compared to stored_h1, which we control. */
    put_u32(machine, BUS_PTR_ADDR, UINT32_C(0x00500000));

    /* Set g6=6 for the v0781 sub-call (production). */
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = 6u;

    /* The dispatcher's first 0x9480 call uses caller-supplied
     * g0/g1/g2 to compute hash1. Set g0 = bus+0x3340 (the
     * 29-byte assignment block), g1=0, g2=29 so the
     * hash matches the data we'll verify. The hash result
     * is then compared to stored_h1, which we control. */
    cpu->registers[VF2_I960_G0_REGISTER + 0u] =
        UINT32_C(0x00503340);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = 29u;

    /* Run the dispatcher. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_record_rebuild_dispatcher_6de3c_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* === Verify stored hashes were updated to true_h1/h2 === */
    CHECK(get_u16(machine, STORED_H1) == true_h1);
    CHECK(get_u16(machine, STORED_H2) == true_h2);

    /* === Verify bus_ptr reset to 0x599000 === */
    CHECK(get_u32(machine, BUS_PTR_ADDR) == UINT32_C(0x00599000));

    /* === Verify mirror copy: 0x503320..0x50333e and
     * 0x503340..0x50335c now hold the correct values
     * (from the assignment/poly records, mirrored via
     * 0x5427c). The mirror copy at 0x599000..0x59bfff
     * also holds the 0x3fe0 source pattern. === */
    /* Check a few bytes in the mirror copy region. */
    CHECK(get_u8(machine, DST_BASE + 0u) ==
          (uint8_t)(fill + 0u));
    CHECK(get_u8(machine, DST_BASE + 1u) ==
          (uint8_t)(fill + 1u));
    CHECK(get_u8(machine, DST_BASE + (COPY_SIZE - 1u)) ==
          (uint8_t)(fill + (uint8_t)((COPY_SIZE - 1u) & 0xffu)));
    /* And the work_ram mirrored records (set by 0x5427c). */
    if (mismatch_h1) {
        /* v0781 wrote assignment block defaults to
         * 0x01d03340..0x35c and 0x5427c mirrored to
         * 0x59c340..0x35c. Verify the first 2 bytes are
         * 2 (MATCH COUNT default). */
        CHECK(get_u8(machine, UINT32_C(0x0059c340)) == 2u);
        CHECK(get_u8(machine, UINT32_C(0x0059c341)) == 2u);
    }
    if (mismatch_h2) {
        /* v0774 wrote 0x01d03320..0x332e and 0x5427c
         * mirrored to 0x59c320..0x32e. v0774 zeros
         * the first 9 bytes, sets last 6 to 2. */
        for (i = 0u; i < 9u; ++i) {
            CHECK(get_u8(machine, UINT32_C(0x0059c320) + i) == 0u);
        }
        for (i = 9u; i < 15u; ++i) {
            CHECK(get_u8(machine, UINT32_C(0x0059c320) + i) == 2u);
        }
    }

    /* === Verify flag bit 0 of 0x500171 set === */
    CHECK((get_u8(machine, FLAG_ADDR) & 0x1u) == 0x1u);

    /* === Verify signature at bus+0x3306..0x3317 === */
    /* bus_ptr was set to 0x1d00000 by block 0x6df6c, then
     * the signature is written, then bus_ptr is reset to
     * 0x599000. So the signature lives in backup_sram. */
    CHECK(get_u16(machine, UINT32_C(0x01d03306)) == 24u);
    CHECK(get_u32(machine, UINT32_C(0x01d03308)) ==
          UINT32_C(0x54524956));
    CHECK(get_u32(machine, UINT32_C(0x01d0330c)) ==
          UINT32_C(0x46204155));
    CHECK(get_u32(machine, UINT32_C(0x01d03310)) ==
          UINT32_C(0x54484749));
    CHECK(get_u32(machine, UINT32_C(0x01d03314)) ==
          UINT32_C(0x32205245));

    /* === Verify zero region 0x500148..0x50014b === */
    for (i = 0u; i < 4u; ++i) {
        CHECK(get_u8(machine, ZERO_BASE + i) == 0u);
    }
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint16_t table[256];

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    CHECK(vf2_model2a_attach_main_data(
        &machine, main_data, MAIN_DATA_SIZE) == VF2_OK);
    /* Pre-fill the CRC table that v0771 reads from
     * main_data 0x02000000 (256 entries, 16 bits each). */
    fill_table_default(table);
    upload_table(table);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* Leg A: both hashes match -> no rebuild, only copy+sig */
    run_leg(&machine, &cpu, 0, 0, 0x11u);
    printf("  Leg A: both match -> no rebuild\n");

    /* Leg B: h1 mismatch -> assignment rebuild */
    run_leg(&machine, &cpu, 1, 0, 0x22u);
    printf("  Leg B: h1 mismatch -> assignment rebuild\n");

    /* Leg C: h2 mismatch -> poly record rebuild */
    run_leg(&machine, &cpu, 0, 1, 0x33u);
    printf("  Leg C: h2 mismatch -> poly record rebuild\n");

    /* Leg D: both mismatch -> both rebuilds */
    run_leg(&machine, &cpu, 1, 1, 0x44u);
    printf("  Leg D: both mismatch -> both rebuilds\n");

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x6de3c (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x6de3c record_rebuild_dispatcher (4 legs)\n");
    return 0;
}
