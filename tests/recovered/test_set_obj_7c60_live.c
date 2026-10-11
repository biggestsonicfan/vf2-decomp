/* ====================================================================
 * 0x7c60 set_obj ctest entries (v0787)
 *
 * Exercises the recovered shipped-symbol function
 * vf2_hybrid_set_obj_7c60_execute, the second cascade step
 * toward the v0784-refused 0x61ba0 body (called at 0x61ba4 and
 * 0x61cc8, plus seven object/display sites):
 *
 * Per vf2i960 disasm (180 B, 5 blocks, ~40 instr):
 *   block_00007c60: ld 0x501018,r4; ld 0x50101c,r5;
 *     cmpobg r5,r4 -> ret if r5 > r4 unsigned (no stores, +4)
 *   block_00007c74: zero/object/FIFO stores, *(g10+0x2008) chase,
 *     0x501010 counter++, main-data quad at 0x020e0004+g0*16,
 *     r3 = r11>>16 then r11 &= 0xffff, cmpobe 0,g1
 *   block_00007cd8 (g1 != 0): r9 += r3*4 (r9 is the ldq value,
 *     so the entry r9 is dead), 0x5010d0 += r11
 *   block_00007cf0: 0x50101c += r11, r11 = 0-1, r8 store,
 *     quad store, ret (+28 for g1 == 0, +32 otherwise)
 *
 * Callers use `call`, callee ends in `ret`: the test pushes a
 * frame per leg and the recovery pops it via
 * vf2_i960_cpu_return_procedure. g0 is clobbered to the table
 * address; g1/g10/g11/g12 are read-only; no local is written
 * back. Final CC/AC published every path (cmpob forms agree
 * across steppers, unlike bbs/bbc).
 *
 * Two modes, one binary:
 *   - native legs (always run, ROM-free): the early path touches
 *     work RAM only; body paths run against a synthetic 1 MB
 *     main-data buffer carrying a known quad at 0x020e0004
 *     (v0776 precedent), so every path is pinned without ROM.
 *     The guard refuses cleanly.
 *   - oracle legs (argv[1] = ROM dir): all three paths against
 *     the reference executor with real ROM + main-data attached;
 *     each must FULL MATCH with identical counts. These arbitrate
 *     the cmpobg/cmpobe directions, the subo order (0-1), the
 *     shro-then-mask order, the dead entry r9, the plain-index
 *     scales and the 4/28/32 totals against live game data.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x00007c60)
#define POST_RET_IP UINT32_C(0x00007d14)
#define G10_BASE    UINT32_C(0x00516000)
#define G10_PTR     UINT32_C(0x00517000)
#define G11_BASE    UINT32_C(0x00518000)
#define G12_OFF     UINT32_C(0x40)
#define CNT_ADDR    UINT32_C(0x00501010)
#define R4_ADDR     UINT32_C(0x00501018)
#define R5_ADDR     UINT32_C(0x0050101c)
#define ACC_ADDR    UINT32_C(0x005010d0)
#define TBL_BASE    UINT32_C(0x020e0004)
#define TBL_IDX     UINT32_C(2)
#define TBL_ADDR    UINT32_C(0x020e0024)
#define SCRUB       UINT32_C(0xaaaaaaaa)
#define AC_INIT     UINT32_C(0x3f001000)
/* Synthetic main-data table (ROM-free body legs): 1 MB covers the
 * quad at 0x020e0004. r8/r9/r10/r11 read back as below, giving
 * r3 = 0x00040005>>16 = 4 and r11 &= 0xffff = 5. */
#define SYNTH_MD_SIZE 0x100000u
#define SYNTH_TBL_OFF 0x0e0004u
#define SYNTH_R8  UINT32_C(0x11111111)
#define SYNTH_R9  UINT32_C(0x00200000)
#define SYNTH_R10 UINT32_C(0x33333333)
#define SYNTH_R11 UINT32_C(0x00040005)
#define SYNTH_R3  UINT32_C(4)
#define SYNTH_R11M UINT32_C(5)
#define SYNTH_R9_B UINT32_C(0x00200010)

static uint8_t synth_md[SYNTH_MD_SIZE];

static void poke_u32le(uint8_t *buf, uint32_t off, uint32_t v) {
    buf[off + 0u] = (uint8_t)(v & 0xffu);
    buf[off + 1u] = (uint8_t)((v >> 8) & 0xffu);
    buf[off + 2u] = (uint8_t)((v >> 16) & 0xffu);
    buf[off + 3u] = (uint8_t)((v >> 24) & 0xffu);
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

static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, a, v) == VF2_OK);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(m, a, &v) == VF2_OK);
    return v;
}

/* Prime one leg: registers, counters, object graph, scrubs, frame. */
static void prime_leg(vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint32_t g1, uint32_t r4v, uint32_t r5v)
{
    put_u32(machine, CNT_ADDR, UINT32_C(100));
    put_u32(machine, ACC_ADDR, UINT32_C(1000));
    put_u32(machine, R4_ADDR, r4v);
    put_u32(machine, R5_ADDR, r5v);
    put_u32(machine, G10_BASE + UINT32_C(0x2008), G10_PTR);
    put_u32(machine, G10_BASE + UINT32_C(0xb0), SCRUB);
    put_u32(machine, G10_BASE + UINT32_C(0x1008), SCRUB);
    put_u32(machine, G10_BASE + UINT32_C(0x10), SCRUB);
    cpu->ip = ENTRY_IP;
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = TBL_IDX;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = g1;
    cpu->registers[VF2_I960_G0_REGISTER + 10u] = G10_BASE;
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = G11_BASE;
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = G12_OFF;
    cpu->compare_result = VF2_I960_COMPARE_NONE;
    cpu->arithmetic_control = AC_INIT;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
}

static void check_frame_post(const vf2_i960_cpu *cpu,
    uint64_t ins0, uint64_t calls0, uint64_t rets0, uint64_t want_insn)
{
    CHECK(cpu->ip == POST_RET_IP);
    CHECK(cpu->local_frame_depth == 0u);
    CHECK(cpu->procedure_calls - calls0 == UINT64_C(0));
    CHECK(cpu->procedure_returns - rets0 == UINT64_C(1));
    CHECK(cpu->executed_instructions - ins0 == want_insn);
}

/* ROM-free native leg: early path (no table touch). */
static void native_early(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    uint64_t ins0;
    uint64_t calls0;
    uint64_t rets0;
    vf2_status status;
    prime_leg(machine, cpu, UINT32_C(7), UINT32_C(10), UINT32_C(20));
    ins0 = cpu->executed_instructions;
    calls0 = cpu->procedure_calls;
    rets0 = cpu->procedure_returns;
    status = vf2_hybrid_set_obj_7c60_execute(machine, cpu);
    CHECK(status == VF2_OK);
    check_frame_post(cpu, ins0, calls0, rets0, UINT64_C(4));
    CHECK(cpu->compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(cpu->arithmetic_control == (AC_INIT | 1u));
    /* Nothing stored, g0 untouched. */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == TBL_IDX);
    CHECK(get_u32(machine, CNT_ADDR) == UINT32_C(100));
    CHECK(get_u32(machine, G10_BASE + UINT32_C(0xb0)) == SCRUB);
    printf("  native L-early: no stores +4 CC GREATER\n");
}

/* ROM-free native body leg against the synthetic table. g1 selects
 * the path (0 -> skip block, else full body). All table-derived
 * values are hand-computed from SYNTH_R8..R11 above. */
static void native_body(vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint32_t g1, uint64_t want_insn,
    vf2_i960_compare_result want_cc, uint32_t want_ac_bits,
    uint32_t want_acc, uint32_t want_quad_r9, const char *name)
{
    uint64_t ins0;
    uint64_t calls0;
    uint64_t rets0;
    uint32_t fifo_base;
    uint32_t quad_base;
    vf2_status status;

    prime_leg(machine, cpu, g1, UINT32_C(20), UINT32_C(10));
    ins0 = cpu->executed_instructions;
    calls0 = cpu->procedure_calls;
    rets0 = cpu->procedure_returns;
    status = vf2_hybrid_set_obj_7c60_execute(machine, cpu);
    CHECK(status == VF2_OK);
    check_frame_post(cpu, ins0, calls0, rets0, want_insn);
    CHECK(cpu->compare_result == want_cc);
    CHECK(cpu->arithmetic_control == (AC_INIT | want_ac_bits));
    /* Fixed stores. */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == TBL_ADDR);
    CHECK(get_u32(machine, CNT_ADDR) == UINT32_C(101));
    CHECK(get_u32(machine, G10_BASE + UINT32_C(0xb0)) == UINT32_C(0));
    CHECK(get_u32(machine, G10_BASE + UINT32_C(0x1008)) ==
        G10_PTR + UINT32_C(0x30));
    CHECK(get_u32(machine, G10_BASE + UINT32_C(0x10)) == SYNTH_R8);
    /* Accumulators and quad store (synthetic-table-derived). */
    CHECK(get_u32(machine, ACC_ADDR) == want_acc);
    CHECK(get_u32(machine, R5_ADDR) ==
        UINT32_C(10) + SYNTH_R11M);
    quad_base = G10_BASE + G12_OFF;
    CHECK(get_u32(machine, quad_base) == SYNTH_R8);
    CHECK(get_u32(machine, quad_base + UINT32_C(4)) == want_quad_r9);
    CHECK(get_u32(machine, quad_base + UINT32_C(8)) == SYNTH_R10);
    CHECK(get_u32(machine, quad_base + UINT32_C(12)) == UINT32_C(0xffffffff));
    /* FIFO keeps the chased pointer (last of the two (g11)[g12]
     * stores; the quad goes to (g10)[g12] instead). */
    fifo_base = G11_BASE + G12_OFF;
    CHECK(get_u32(machine, fifo_base) == G10_PTR);
    printf("  native %s: body ok +%llu\n", name,
        (unsigned long long)want_insn);
}

static void guard_leg(vf2_model2a *machine, vf2_i960_cpu *cpu) {    uint64_t before;
    vf2_status status;
    prime_leg(machine, cpu, UINT32_C(7), UINT32_C(10), UINT32_C(20));
    /* The guard refuses before touching state; the pushed test frame
     * belongs to the harness and is discarded with the cpu. */
    cpu->ip = UINT32_C(0x00007c68);
    before = cpu->executed_instructions;
    status = vf2_hybrid_set_obj_7c60_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->executed_instructions - before == UINT64_C(0));
    CHECK(get_u32(machine, CNT_ADDR) == UINT32_C(100));
    CHECK(get_u32(machine, G10_BASE + UINT32_C(0xb0)) == SCRUB);
    printf("  native L-guard: refused, no side effects\n");
}

/* ROM-backed oracle leg: reference steps against native, FULL MATCH.
 * want_g1 selects the path (0 -> skip block, else full body);
 * want_insn arbitrates the 4/28/32 totals; want_cc is GREATER for
 * the early exit, EQUAL for g1 == 0, LESS otherwise. */
static void oracle_leg(const uint8_t *rom, size_t rom_sz,
    const uint8_t *data, size_t data_sz,
    uint32_t g1, uint32_t r4v, uint32_t r5v, uint64_t want_insn,
    vf2_i960_compare_result want_cc, uint32_t want_ac_bits,
    const char *name)
{
    vf2_model2a ref_m;
    vf2_model2a nat_m;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu nat_cpu;
    vf2_i960_snapshot_diff diff;
    uint64_t ref_ins0;
    uint64_t nat_ins0;
    uint64_t ref_calls0;
    uint64_t ref_rets0;
    uint64_t nat_calls0;
    uint64_t nat_rets0;
    size_t steps = 0u;
    vf2_status status;

    memset(&ref_m, 0, sizeof(ref_m));
    memset(&nat_m, 0, sizeof(nat_m));
    memset(&ref_cpu, 0, sizeof(ref_cpu));
    memset(&nat_cpu, 0, sizeof(nat_cpu));
    memset(&diff, 0, sizeof(diff));

    CHECK(vf2_model2a_initialize(&ref_m) != 0);
    CHECK(vf2_model2a_initialize(&nat_m) != 0);
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rom_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rom_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, data_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, data_sz) == VF2_OK);
    vf2_i960_cpu_reset(&ref_cpu, 0u, 0u, ENTRY_IP);
    vf2_i960_cpu_reset(&nat_cpu, 0u, 0u, ENTRY_IP);
    prime_leg(&ref_m, &ref_cpu, g1, r4v, r5v);
    prime_leg(&nat_m, &nat_cpu, g1, r4v, r5v);

    ref_ins0 = ref_cpu.executed_instructions;
    nat_ins0 = nat_cpu.executed_instructions;
    ref_calls0 = ref_cpu.procedure_calls;
    ref_rets0 = ref_cpu.procedure_returns;
    nat_calls0 = nat_cpu.procedure_calls;
    nat_rets0 = nat_cpu.procedure_returns;

    /* Drive the oracle through the ret; never step past it. */
    while (ref_cpu.ip != POST_RET_IP && steps < 48u) {
        status = vf2_i960_step(&ref_cpu, &ref_m, NULL);
        CHECK(status == VF2_OK);
        if (status != VF2_OK) {
            break;
        }
        ++steps;
    }
    CHECK(ref_cpu.ip == POST_RET_IP);

    status = vf2_hybrid_set_obj_7c60_execute(&nat_m, &nat_cpu);
    CHECK(status == VF2_OK);

    /* The oracle arbitrates the hand-counted total. */
    CHECK(ref_cpu.executed_instructions - ref_ins0 == want_insn);
    CHECK(nat_cpu.executed_instructions - nat_ins0 ==
        (ref_cpu.executed_instructions - ref_ins0));
    CHECK(ref_cpu.procedure_calls - ref_calls0 == UINT64_C(0));
    CHECK(ref_cpu.procedure_returns - ref_rets0 == UINT64_C(1));

    /* Primed-derived poststate, named per component. */
    check_frame_post(&nat_cpu, nat_ins0, nat_calls0, nat_rets0, want_insn);
    CHECK(nat_cpu.compare_result == want_cc);
    CHECK(nat_cpu.arithmetic_control == (AC_INIT | want_ac_bits));
    CHECK(nat_cpu.registers[VF2_I960_G0_REGISTER + 1u] == g1);
    CHECK(nat_cpu.registers[VF2_I960_G0_REGISTER + 10u] == G10_BASE);
    CHECK(nat_cpu.registers[VF2_I960_G0_REGISTER + 11u] == G11_BASE);
    CHECK(nat_cpu.registers[VF2_I960_G0_REGISTER + 12u] == G12_OFF);
    if (r5v > r4v) {
        CHECK(nat_cpu.registers[VF2_I960_G0_REGISTER + 0u] == TBL_IDX);
        CHECK(get_u32(&nat_m, CNT_ADDR) == UINT32_C(100));
        CHECK(get_u32(&nat_m, G10_BASE + UINT32_C(0xb0)) == SCRUB);
    } else {
        CHECK(nat_cpu.registers[VF2_I960_G0_REGISTER + 0u] == TBL_ADDR);
        CHECK(get_u32(&nat_m, CNT_ADDR) == UINT32_C(101));
        CHECK(get_u32(&nat_m, G10_BASE + UINT32_C(0xb0)) == UINT32_C(0));
        CHECK(get_u32(&nat_m, G10_BASE + UINT32_C(0x1008)) ==
            G10_PTR + UINT32_C(0x30));
    }

    CHECK(vf2_i960_compare_live_state(&ref_cpu, &ref_m, &nat_cpu, &nat_m,
                                      &diff) == VF2_OK);
    if (!diff.equal) {
        fprintf(stderr,
            "MISMATCH %s live state: component=%s bytes=%u "
            "first_offset=0x%zx expected=0x%08x actual=0x%08x\n",
            name, diff.component, (unsigned)diff.differing_bytes,
            diff.first_offset, diff.expected_value, diff.actual_value);
        ++failures;
    } else {
        printf("  oracle %s: FULL MATCH (%llu insn)\n", name,
            (unsigned long long)(ref_cpu.executed_instructions - ref_ins0));
    }

    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
}

int main(int argc, char **argv) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* Synthetic main-data table for the ROM-free body legs. The
     * recovery reads entry TBL_IDX (offset +idx*16 words), so poke
     * the quad there, not at the table base. */
    memset(synth_md, 0, sizeof(synth_md));
    poke_u32le(synth_md, SYNTH_TBL_OFF + (uint32_t)TBL_IDX * 16u,
        SYNTH_R8);
    poke_u32le(synth_md, SYNTH_TBL_OFF + (uint32_t)TBL_IDX * 16u + 4u,
        SYNTH_R9);
    poke_u32le(synth_md, SYNTH_TBL_OFF + (uint32_t)TBL_IDX * 16u + 8u,
        SYNTH_R10);
    poke_u32le(synth_md, SYNTH_TBL_OFF + (uint32_t)TBL_IDX * 16u + 12u,
        SYNTH_R11);
    CHECK(vf2_model2a_attach_main_data(
        &machine, synth_md, sizeof(synth_md)) == VF2_OK);

    native_early(&machine, &cpu);
    /* g1 == 0: skips the 0x5010d0 block, +28, CC EQUAL. */
    native_body(&machine, &cpu, UINT32_C(0), UINT64_C(28),
        VF2_I960_COMPARE_EQUAL, 2u, UINT32_C(1000), SYNTH_R9, "L-skip");
    /* g1 != 0: full body, +32, CC LESS. */
    native_body(&machine, &cpu, UINT32_C(7), UINT64_C(32),
        VF2_I960_COMPARE_LESS, 4u,
        UINT32_C(1000) + SYNTH_R11M, SYNTH_R9_B, "L-full");
    guard_leg(&machine, &cpu);
    vf2_model2a_shutdown(&machine);

    if (argc == 2) {
        uint8_t *rom = NULL;
        size_t rom_sz = 0u;
        uint8_t *data = NULL;
        size_t data_sz = 0u;
        if (vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                    &rom, &rom_sz) != VF2_OK ||
            vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                    &data, &data_sz) != VF2_OK) {
            fprintf(stderr, "FAILED: could not build ROM regions\n");
            free(rom);
            free(data);
            return 1;
        }
        if (rom_sz <= (size_t)0x7d14) {
            fprintf(stderr, "FAILED: main CPU region too small\n");
            free(rom);
            free(data);
            return 1;
        }
        /* Early exit (no stores). */
        oracle_leg(rom, rom_sz, data, data_sz,
            UINT32_C(7), UINT32_C(10), UINT32_C(20), UINT64_C(4),
            VF2_I960_COMPARE_GREATER, 1u, "L-early");
        /* Full body minus block (g1 == 0). */
        oracle_leg(rom, rom_sz, data, data_sz,
            UINT32_C(0), UINT32_C(20), UINT32_C(10), UINT64_C(28),
            VF2_I960_COMPARE_EQUAL, 2u, "L-skip");
        /* Full body (g1 != 0). */
        oracle_leg(rom, rom_sz, data, data_sz,
            UINT32_C(7), UINT32_C(20), UINT32_C(10), UINT64_C(32),
            VF2_I960_COMPARE_LESS, 4u, "L-full");
        free(rom);
        free(data);
    } else {
        printf("  (oracle legs skipped: no ROM dir)\n");
    }

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x7c60 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x7c60 set_obj\n");
    return 0;
}
