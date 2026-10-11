/* ====================================================================
 * 0x53884 reject_sample ctest entries (v0785)
 *
 * Exercises the recovered shipped-symbol function
 * vf2_hybrid_reject_sample_53884_execute, the second recovery
 * from the v0783 87 KB gap characterisation:
 *
 * Per vf2i960 disasm (96 B, 6 blocks, 18 instr):
 *   00053884  ldob     0x0000005c(g13)[g1], r12
 *   0005388c  cmpo     8, r12
 *   00053890  be       0x000538e0
 *   00053894  ldob     0x00050910, r14
 *   0005389c  ldob     0x00050911, r15
 *   000538a4  cmpo     r15, r12
 *   000538a8  concmpo  r14, r12
 *   000538ac  be       0x000538d4
 *   000538b0  ldob     0x000001b1(g7), r13
 *   000538b4  ldob     0x00050912[r13*2], r14
 *   000538bc  ldob     0x00050913[r13*2], r15
 *   000538c4  cmpo     r15, r12
 *   000538c8  concmpo  r14, r12
 *   000538cc  be       0x000538d4
 *   000538d0  b        0x000538e0
 *   000538d4  lda      0x000000ff, r12
 *   000538d8  stob     r12, 0x0000005c(g13)[g1]
 *   000538e0  bx       (g14)
 *
 * The sole caller uses `bal` (0x5219c in make_action_table), so no
 * frame is pushed or popped: locals persist, procedure counters do
 * not move, and the exit ip is g14's entry value (here the true
 * caller resume 0x521a0).
 *
 * ADDRESS-SPACE NOTE (fixture bug caught by the oracle during v0785):
 * the pair/table bytes at 0x00050910..0x00050913 are MAIN-ROM
 * constants (below 2 MB), not work RAM. The 8-digit form is
 * 0x00050910, not 0x00500910 -- one dropped zero routes the access
 * to a different 4 MB page. The first version of this slice primed
 * work-RAM 0x500910 and read it back self-consistently, so every
 * native leg passed while the reference read the real ROM bytes
 * (0x07/0x0c/0x4c/0x4f at idx 3). Only the ROM-backed legs caught
 * it. Consequences drawn:
 *   - the recovery reads the ROM addresses (fixed here);
 *   - pair/table bytes are read-only: only the sentinel path (L1)
 *     and the entry guard (L7) are ROM-free, because every other
 *     path loads ROM constants;
 *   - oracle legs derive expectations from the attached ROM bytes
 *     at runtime and premise-guard every leg shape (a degenerate
 *     table, e.g. R0 == R1, fails loudly instead of silently
 *     covering the wrong path).
 *
 * Two modes, one binary:
 *   - native legs (always run, ROM-free): L1 sentinel keep + L7
 *     entry-guard control, hand-computed expectations.
 *   - oracle legs (argv[1] = ROM dir): L1..L6G run against the
 *     reference executor with the real ROM attached; each must FULL
 *     MATCH via vf2_i960_compare_live_state (registers incl. locals,
 *     CC/AC, frames, memory, counters) with identical instruction
 *     counts. These legs prove the indexed scale, the concmpo
 *     preserve/replace arms, the published CC/AC and the ROM
 *     addresses above.
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

#define ENTRY_IP    UINT32_C(0x00053884)
#define RESUME_IP   UINT32_C(0x000521a0)
#define SAMPLE_BASE UINT32_C(0x00513000)
#define G7_BASE     UINT32_C(0x00510000)
#define G1_INDEX    UINT32_C(2)
#define SAMPLE_ADDR UINT32_C(0x0051305e)
#define IDX_ADDR    UINT32_C(0x005101b1)
/* ROM-resident constant tables (NOT work RAM -- see note above). */
#define ROM_PAIR0   UINT32_C(0x00050910)
#define ROM_PAIR1   UINT32_C(0x00050911)
#define ROM_TBL0    UINT32_C(0x00050912)
#define ROM_TBL1    UINT32_C(0x00050913)
#define TBL_INDEX   UINT8_C(3)
#define MISS_HI     UINT8_C(0x77)
#define MISS_LO     UINT8_C(0x40)
#define AC_INIT     UINT32_C(0x3f001000)
#define R12_SENTINEL UINT32_C(0xa11ce012)
#define R13_SENTINEL UINT32_C(0xa11ce013)
#define R14_SENTINEL UINT32_C(0xa11ce014)
#define R15_SENTINEL UINT32_C(0xa11ce015)

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
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

/* Prime the work-RAM half of one leg (sample byte, index byte) and
 * the registers identically on any machine/cpu pair. The pair/table
 * bytes come from ROM and are never written. */
static void prime_work(vf2_model2a *machine, vf2_i960_cpu *cpu,
    uint8_t sample)
{
    put_u8(machine, SAMPLE_ADDR, sample);
    put_u8(machine, IDX_ADDR, TBL_INDEX);
    cpu->ip = ENTRY_IP;
    cpu->registers[VF2_I960_G0_REGISTER + 13u] = SAMPLE_BASE;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = G1_INDEX;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = G7_BASE;
    cpu->registers[VF2_I960_G14_REGISTER] = RESUME_IP;
    cpu->registers[12u] = R12_SENTINEL;
    cpu->registers[13u] = R13_SENTINEL;
    cpu->registers[14u] = R14_SENTINEL;
    cpu->registers[15u] = R15_SENTINEL;
    cpu->compare_result = VF2_I960_COMPARE_NONE;
    cpu->arithmetic_control = AC_INIT;
}

static void check_common_post(const vf2_i960_cpu *cpu, uint64_t before,
    uint64_t want_insn, uint32_t want_r12, uint32_t want_r13,
    uint32_t want_r14, uint32_t want_r15,
    vf2_i960_compare_result want_cc, uint32_t want_ac_bits)
{
    CHECK(cpu->ip == RESUME_IP);
    CHECK(cpu->registers[12u] == want_r12);
    CHECK(cpu->registers[13u] == want_r13);
    CHECK(cpu->registers[14u] == want_r14);
    CHECK(cpu->registers[15u] == want_r15);
    /* Globals untouched (bal-frame convention). */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 13u] == SAMPLE_BASE);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 1u] == G1_INDEX);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 7u] == G7_BASE);
    CHECK(cpu->registers[VF2_I960_G14_REGISTER] == RESUME_IP);
    /* No frame pushed, no procedure counted. */
    CHECK(cpu->local_frame_depth == 0u);
    CHECK(cpu->procedure_calls == 0u);
    CHECK(cpu->procedure_returns == 0u);
    CHECK(cpu->executed_instructions - before == want_insn);
    CHECK(cpu->compare_result == want_cc);
    CHECK(cpu->arithmetic_control == (AC_INIT | want_ac_bits));
}

/* ROM-free native leg: L1 sentinel keep (touches no ROM address). */
static void native_l1(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    uint64_t before;
    vf2_status status;
    prime_work(machine, cpu, UINT8_C(8));
    before = cpu->executed_instructions;
    status = vf2_hybrid_reject_sample_53884_execute(machine, cpu);
    CHECK(status == VF2_OK);
    check_common_post(cpu, before, UINT64_C(4),
        UINT32_C(8), R13_SENTINEL, R14_SENTINEL, R15_SENTINEL,
        VF2_I960_COMPARE_EQUAL, 2u);
    CHECK(get_u8(machine, SAMPLE_ADDR) == UINT8_C(8));
    printf("  native L1-sentinel: keep +4\n");
}

static void guard_leg(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    uint64_t before;
    vf2_status status;
    prime_work(machine, cpu, UINT8_C(8));
    cpu->ip = UINT32_C(0x00053888);
    before = cpu->executed_instructions;
    status = vf2_hybrid_reject_sample_53884_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->registers[12u] == R12_SENTINEL);
    CHECK(cpu->registers[13u] == R13_SENTINEL);
    CHECK(cpu->registers[14u] == R14_SENTINEL);
    CHECK(cpu->registers[15u] == R15_SENTINEL);
    CHECK(cpu->executed_instructions - before == UINT64_C(0));
    CHECK(get_u8(machine, SAMPLE_ADDR) == UINT8_C(8));
    printf("  native L7-guard: refused, no side effects\n");
}

/* Guard the premise of each oracle leg: a degenerate ROM table must
 * fail loudly instead of silently covering the wrong path. */
static int check_premises(uint8_t r0, uint8_t r1, uint8_t t0, uint8_t t1) {
    int ok = 1;
#define PREMISE(cond, text)                                           \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAILED premise: %s "                     \
                "(r0=0x%02x r1=0x%02x t0=0x%02x t1=0x%02x)\n",         \
                text, r0, r1, t0, t1);                               \
            ok = 0;                                                   \
        }                                                             \
    } while (0)
    PREMISE(r0 != r1, "first pair degenerate (L3 needs R0 != R1)");
    PREMISE(r0 != 8u && r1 != 8u && t0 != 8u && t1 != 8u,
        "table holds the sentinel (legs would exit early)");
    PREMISE(t1 != r0 && t1 != r1, "L4 sample hits the first pair");
    PREMISE(t0 != r0 && t0 != r1 && t0 != t1, "L5 sample mis-paths");
    PREMISE(MISS_HI != 8u && MISS_HI != r0 && MISS_HI != r1 &&
        MISS_HI != t0 && MISS_HI != t1, "L6 sample is not a miss");
    PREMISE(MISS_LO != 8u && MISS_LO != r0 && MISS_LO != r1 &&
        MISS_LO != t0 && MISS_LO != t1, "L6G sample is not a miss");
    PREMISE(t0 < MISS_HI, "L6 polarity is not LESS");
    PREMISE(t0 > MISS_LO, "L6G polarity is not GREATER");
#undef PREMISE
    if (ok != 0) {
        fprintf(stderr, "fixture premises hold "
            "(r0=0x%02x r1=0x%02x t0=0x%02x t1=0x%02x)\n", r0, r1, t0, t1);
    }
    return ok;
}

/* ROM-backed oracle leg: reference steps against native, FULL MATCH. */
static void oracle_leg(const uint8_t *rom, size_t rom_sz, uint8_t sample,
    uint32_t want_r12, uint32_t want_r13, uint32_t want_r14,
    uint32_t want_r15, uint64_t want_insn,
    vf2_i960_compare_result want_cc, uint32_t want_ac_bits,
    uint8_t want_mem, const char *name)
{
    vf2_model2a ref_m;
    vf2_model2a nat_m;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu nat_cpu;
    vf2_i960_snapshot_diff diff;
    uint64_t ref_ins0;
    uint64_t nat_ins0;
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
    vf2_i960_cpu_reset(&ref_cpu, 0u, 0u, ENTRY_IP);
    vf2_i960_cpu_reset(&nat_cpu, 0u, 0u, ENTRY_IP);
    prime_work(&ref_m, &ref_cpu, sample);
    prime_work(&nat_m, &nat_cpu, sample);

    ref_ins0 = ref_cpu.executed_instructions;
    nat_ins0 = nat_cpu.executed_instructions;

    /* Drive the oracle to the bal resume; never step past it. */
    while (ref_cpu.ip != RESUME_IP && steps < 64u) {
        status = vf2_i960_step(&ref_cpu, &ref_m, NULL);
        CHECK(status == VF2_OK);
        if (status != VF2_OK) {
            break;
        }
        ++steps;
    }
    CHECK(ref_cpu.ip == RESUME_IP);

    status = vf2_hybrid_reject_sample_53884_execute(&nat_m, &nat_cpu);
    CHECK(status == VF2_OK);

    /* The oracle confirms the hand-counted instruction total. */
    CHECK(ref_cpu.executed_instructions - ref_ins0 == want_insn);
    CHECK(nat_cpu.executed_instructions - nat_ins0 ==
        (ref_cpu.executed_instructions - ref_ins0));
    /* bal/bx move no procedure counters. */
    CHECK(ref_cpu.procedure_calls == 0u);
    CHECK(ref_cpu.procedure_returns == 0u);

    /* Native poststate matches the hand computation first (names the
     * component on failure), then the full oracle comparison. */
    check_common_post(&nat_cpu, nat_ins0, want_insn,
        want_r12, want_r13, want_r14, want_r15, want_cc, want_ac_bits);
    CHECK(get_u8(&nat_m, SAMPLE_ADDR) == want_mem);

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

    native_l1(&machine, &cpu);
    guard_leg(&machine, &cpu);
    vf2_model2a_shutdown(&machine);

    if (argc == 2) {
        uint8_t *rom = NULL;
        size_t rom_sz = 0u;
        uint8_t r0;
        uint8_t r1;
        uint8_t t0;
        uint8_t t1;
        if (vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                    &rom, &rom_sz) != VF2_OK) {
            fprintf(stderr, "FAILED: could not build the main CPU region\n");
            free(rom);
            return 1;
        }
        if (rom_sz <= (size_t)0x538e4 || rom_sz <= (size_t)0x50919) {
            fprintf(stderr, "FAILED: main CPU region too small\n");
            free(rom);
            return 1;
        }
        r0 = rom[0x50910];
        r1 = rom[0x50911];
        t0 = rom[0x50912 + (size_t)TBL_INDEX * 2u];
        t1 = rom[0x50913 + (size_t)TBL_INDEX * 2u];
        if (check_premises(r0, r1, t0, t1) == 0) {
            ++failures;
            free(rom);
            return 1;
        }
        /* L1: sentinel keep. */
        oracle_leg(rom, rom_sz, UINT8_C(8),
            UINT32_C(8), R13_SENTINEL, R14_SENTINEL, R15_SENTINEL,
            UINT64_C(4), VF2_I960_COMPARE_EQUAL, 2u, UINT8_C(8),
            "L1-sentinel");
        /* L2: first-pair cmpo hit (sample == mem[0x50911]). */
        oracle_leg(rom, rom_sz, r1,
            UINT32_C(0xff), R13_SENTINEL, (uint32_t)r0, (uint32_t)r1,
            UINT64_C(11), VF2_I960_COMPARE_EQUAL, 2u, UINT8_C(0xff),
            "L2-pair0-cmpo");
        /* L3: first-pair concmpo hit (sample == mem[0x50910] only). */
        oracle_leg(rom, rom_sz, r0,
            UINT32_C(0xff), R13_SENTINEL, (uint32_t)r0, (uint32_t)r1,
            UINT64_C(11), VF2_I960_COMPARE_EQUAL, 2u, UINT8_C(0xff),
            "L3-pair0-conccmpo");
        /* L4: second-pair cmpo hit. */
        oracle_leg(rom, rom_sz, t1,
            UINT32_C(0xff), (uint32_t)TBL_INDEX, (uint32_t)t0, (uint32_t)t1,
            UINT64_C(17), VF2_I960_COMPARE_EQUAL, 2u, UINT8_C(0xff),
            "L4-pair1-cmpo");
        /* L5: second-pair concmpo hit. */
        oracle_leg(rom, rom_sz, t0,
            UINT32_C(0xff), (uint32_t)TBL_INDEX, (uint32_t)t0, (uint32_t)t1,
            UINT64_C(17), VF2_I960_COMPARE_EQUAL, 2u, UINT8_C(0xff),
            "L5-pair1-conccmpo");
        /* L6: full miss above the table byte -> CC LESS. */
        oracle_leg(rom, rom_sz, MISS_HI,
            (uint32_t)MISS_HI, (uint32_t)TBL_INDEX, (uint32_t)t0, (uint32_t)t1,
            UINT64_C(16), VF2_I960_COMPARE_LESS, 4u, MISS_HI,
            "L6-miss-less");
        /* L6G: full miss below the table byte -> CC GREATER. */
        oracle_leg(rom, rom_sz, MISS_LO,
            (uint32_t)MISS_LO, (uint32_t)TBL_INDEX, (uint32_t)t0, (uint32_t)t1,
            UINT64_C(16), VF2_I960_COMPARE_GREATER, 1u, MISS_LO,
            "L6G-miss-greater");
        free(rom);
    } else {
        printf("  (oracle legs skipped: no ROM dir)\n");
    }

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x53884 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x53884 reject_sample\n");
    return 0;
}
