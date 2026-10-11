/* ====================================================================
 * 0x7b18 set_mmode ctest entries (v0786)
 *
 * Exercises the recovered shipped-symbol function
 * vf2_hybrid_set_mmode_7b18_execute, the first cascade step
 * toward the v0784-refused 0x61ba0 body (called at 0x61ba4 and
 * 0x61cf4, plus 0x117fc/0x11804):
 *
 * Per vf2i960 disasm (16 B, 1 block, 4 instr):
 *   00007b18  mov      0, r3
 *   00007b1c  st       r3, 0x00000070(g10)
 *   00007b20  st       g0, (g10)[g12]
 *   00007b24  ret
 *
 * All four callers use the dedicated `call` instruction and the
 * callee ends in the dedicated `ret`, so the call/ret frame
 * discipline applies (verified, not assumed, after the v0785
 * bal/bx lesson): the test enters a procedure frame per leg and
 * the recovery pops it via vf2_i960_cpu_return_procedure. The
 * `mov 0, r3` evaporates with the frame. No instruction writes
 * condition state, so CC/AC pass through untouched.
 *
 * Two modes, one binary:
 *   - native legs (always run, ROM-free): the body touches work
 *     RAM only, so every leg runs without ROM against
 *     hand-computed expectations. Target words are scrubbed with
 *     0xAAAAAAAA first so a missing store cannot hide behind a
 *     stale zero.
 *   - oracle legs (argv[1] = ROM dir): the same legs run against
 *     the reference executor with the real ROM attached (code
 *     fetch only); each must FULL MATCH via
 *     vf2_i960_compare_live_state with identical counts. These
 *     arbitrate the plain-index scale (`(g10)[g12]` read as
 *     g10+g12) and the body+ret instruction total.
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

#define ENTRY_IP    UINT32_C(0x00007b18)
#define POST_RET_IP UINT32_C(0x00007b28)
#define SCRUB       UINT32_C(0xaaaaaaaa)
#define AC_INIT     UINT32_C(0x3f001000)

static int failures = 0;
#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            fprintf(stderr, "FAILED %s:%d: %s\n",                     \
                __FILE__, __LINE__, #expr);                           \
            ++failures;                                               \
        }                                                             \
    } while (0)

typedef struct mmode_leg {
    const char *name;
    uint32_t g10;
    uint32_t g12;
    uint32_t g0;
} mmode_leg;

static const mmode_leg legs[] = {
    {"L1-basic", UINT32_C(0x00514000), UINT32_C(5), UINT32_C(0x12345678)},
    {"L2-index0-max", UINT32_C(0x00514000), UINT32_C(0), UINT32_C(0xffffffff)},
    {"L3-far-index", UINT32_C(0x00515000), UINT32_C(0x100), UINT32_C(1)},
};
#define LEG_COUNT (sizeof(legs) / sizeof(legs[0]))

static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, a, v) == VF2_OK);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(m, a, &v) == VF2_OK);
    return v;
}

/* Prime one leg: scrub both target words, pin registers + CC/AC,
 * push the callee frame. */
static void prime_leg(vf2_model2a *machine, vf2_i960_cpu *cpu,
    const mmode_leg *leg)
{
    put_u32(machine, leg->g10 + UINT32_C(0x70), SCRUB);
    put_u32(machine, leg->g10 + leg->g12, SCRUB);
    cpu->ip = ENTRY_IP;
    cpu->registers[VF2_I960_G0_REGISTER + 10u] = leg->g10;
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = leg->g12;
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = leg->g0;
    cpu->compare_result = VF2_I960_COMPARE_NONE;
    cpu->arithmetic_control = AC_INIT;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
}

static void check_post(vf2_model2a *machine, vf2_i960_cpu *cpu,
    const mmode_leg *leg, uint64_t ins0, uint64_t calls0, uint64_t rets0)
{
    CHECK(cpu->ip == POST_RET_IP);
    /* Globals read, never written. */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 10u] == leg->g10);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 12u] == leg->g12);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == leg->g0);
    /* The two stores. */
    CHECK(get_u32(machine, leg->g10 + UINT32_C(0x70)) == UINT32_C(0));
    CHECK(get_u32(machine, leg->g10 + leg->g12) == leg->g0);
    /* Frame popped (the test's enter_procedure pushed it), no new
     * call made by the body, exactly one return. */
    CHECK(cpu->local_frame_depth == 0u);
    CHECK(cpu->procedure_calls - calls0 == UINT64_C(0));
    CHECK(cpu->procedure_returns - rets0 == UINT64_C(1));
    CHECK(cpu->executed_instructions - ins0 == UINT64_C(4));
    /* No condition state written. */
    CHECK(cpu->compare_result == VF2_I960_COMPARE_NONE);
    CHECK(cpu->arithmetic_control == AC_INIT);
}

/* ROM-free native leg against hand-computed expectations. */
static void native_leg(vf2_model2a *machine, vf2_i960_cpu *cpu,
    const mmode_leg *leg)
{
    uint64_t ins0;
    uint64_t calls0;
    uint64_t rets0;
    vf2_status status;

    prime_leg(machine, cpu, leg);
    ins0 = cpu->executed_instructions;
    calls0 = cpu->procedure_calls;
    rets0 = cpu->procedure_returns;
    status = vf2_hybrid_set_mmode_7b18_execute(machine, cpu);
    CHECK(status == VF2_OK);
    check_post(machine, cpu, leg, ins0, calls0, rets0);
    printf("  native %s: stores ok +4\n", leg->name);
}

/* ROM-backed oracle leg: reference steps against native, FULL MATCH. */
static void oracle_leg(const uint8_t *rom, size_t rom_sz,
    const mmode_leg *leg)
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
    vf2_i960_cpu_reset(&ref_cpu, 0u, 0u, ENTRY_IP);
    vf2_i960_cpu_reset(&nat_cpu, 0u, 0u, ENTRY_IP);
    prime_leg(&ref_m, &ref_cpu, leg);
    prime_leg(&nat_m, &nat_cpu, leg);

    ref_ins0 = ref_cpu.executed_instructions;
    nat_ins0 = nat_cpu.executed_instructions;
    ref_calls0 = ref_cpu.procedure_calls;
    ref_rets0 = ref_cpu.procedure_returns;
    nat_calls0 = nat_cpu.procedure_calls;
    nat_rets0 = nat_cpu.procedure_returns;

    /* Drive the oracle through the ret; never step past it. */
    while (ref_cpu.ip != POST_RET_IP && steps < 16u) {
        status = vf2_i960_step(&ref_cpu, &ref_m, NULL);
        CHECK(status == VF2_OK);
        if (status != VF2_OK) {
            break;
        }
        ++steps;
    }
    CHECK(ref_cpu.ip == POST_RET_IP);

    status = vf2_hybrid_set_mmode_7b18_execute(&nat_m, &nat_cpu);
    CHECK(status == VF2_OK);

    /* The oracle arbitrates the body+ret instruction total. */
    CHECK(ref_cpu.executed_instructions - ref_ins0 == UINT64_C(4));
    CHECK(nat_cpu.executed_instructions - nat_ins0 ==
        (ref_cpu.executed_instructions - ref_ins0));
    CHECK(ref_cpu.procedure_calls - ref_calls0 == UINT64_C(0));
    CHECK(ref_cpu.procedure_returns - ref_rets0 == UINT64_C(1));

    check_post(&nat_m, &nat_cpu, leg, nat_ins0, nat_calls0, nat_rets0);

    CHECK(vf2_i960_compare_live_state(&ref_cpu, &ref_m, &nat_cpu, &nat_m,
                                      &diff) == VF2_OK);
    if (!diff.equal) {
        fprintf(stderr,
            "MISMATCH %s live state: component=%s bytes=%u "
            "first_offset=0x%zx expected=0x%08x actual=0x%08x\n",
            leg->name, diff.component, (unsigned)diff.differing_bytes,
            diff.first_offset, diff.expected_value, diff.actual_value);
        ++failures;
    } else {
        printf("  oracle %s: FULL MATCH (%llu insn)\n", leg->name,
            (unsigned long long)(ref_cpu.executed_instructions - ref_ins0));
    }

    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
}

static void guard_leg(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    uint64_t before;
    vf2_status status;
    prime_leg(machine, cpu, &legs[0]);
    /* The guard refuses before touching state; the frame prime_leg
     * pushed belongs to the harness and is discarded with the cpu. */
    cpu->ip = UINT32_C(0x00007b1c);
    before = cpu->executed_instructions;
    status = vf2_hybrid_set_mmode_7b18_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->executed_instructions - before == UINT64_C(0));
    CHECK(get_u32(machine, legs[0].g10 + UINT32_C(0x70)) == SCRUB);
    CHECK(get_u32(machine, legs[0].g10 + legs[0].g12) == SCRUB);
    printf("  native L7-guard: refused, no side effects\n");
}

int main(int argc, char **argv) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    size_t i;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    for (i = 0u; i < LEG_COUNT; ++i) {
        native_leg(&machine, &cpu, &legs[i]);
    }
    guard_leg(&machine, &cpu);
    vf2_model2a_shutdown(&machine);

    if (argc == 2) {
        uint8_t *rom = NULL;
        size_t rom_sz = 0u;
        if (vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                    &rom, &rom_sz) != VF2_OK) {
            fprintf(stderr, "FAILED: could not build the main CPU region\n");
            free(rom);
            return 1;
        }
        if (rom_sz <= (size_t)0x7b28) {
            fprintf(stderr, "FAILED: main CPU region too small\n");
            free(rom);
            return 1;
        }
        for (i = 0u; i < LEG_COUNT; ++i) {
            oracle_leg(rom, rom_sz, &legs[i]);
        }
        free(rom);
    } else {
        printf("  (oracle legs skipped: no ROM dir)\n");
    }

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x7b18 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x7b18 set_mmode\n");
    return 0;
}
