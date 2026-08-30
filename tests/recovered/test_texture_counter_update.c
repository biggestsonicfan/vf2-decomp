#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"

static int failures = 0;

#define CHECK(expr)                                           \
    do {                                                      \
        if (!(expr)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++failures;                                       \
        }                                                     \
    } while (0)

#define COUNTER0 UINT32_C(0x005502c0)
#define COUNTER1 UINT32_C(0x005502d0)
#define COUNTER2 UINT32_C(0x005502e0)
#define STATUS_WORD UINT32_C(0x0055c2f0)
#define ENTRY UINT32_C(0x0004bb98)
#define EXIT UINT32_C(0x0004bc58)
#define SENTINEL UINT32_C(0x00012340)

static void seed_u32(vf2_model2a *m, uint32_t addr, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, addr, v) == VF2_OK);
}
static void seed_u16(vf2_model2a *m, uint32_t addr, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xff), (uint8_t)(v >> 8)};
    CHECK(vf2_model2a_write(m, addr, b, sizeof(b)) == VF2_OK);
}

static vf2_hybrid_bridge_report run_counter_case(uint32_t c0, uint32_t c1, uint32_t c2, uint16_t status_word, uint32_t arg0_c0, uint32_t arg0_c1, uint32_t arg0_c2) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;
    memset(&report, 0, sizeof(report));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    // seed counters and their argument triples
    seed_u32(&machine, COUNTER0, c0);
    seed_u32(&machine, COUNTER0 + 4, arg0_c0);
    seed_u32(&machine, COUNTER0 + 8, 0);
    seed_u32(&machine, COUNTER0 + 12, 0);
    seed_u32(&machine, COUNTER1, c1);
    seed_u32(&machine, COUNTER1 + 4, arg0_c1);
    seed_u32(&machine, COUNTER1 + 8, 0);
    seed_u32(&machine, COUNTER1 + 12, 0);
    seed_u32(&machine, COUNTER2, c2);
    seed_u32(&machine, COUNTER2 + 4, arg0_c2);
    seed_u32(&machine, COUNTER2 + 8, 0);
    seed_u32(&machine, COUNTER2 + 12, 0);
    seed_u16(&machine, STATUS_WORD, status_word);
    // also seed tile ram destinations for diagnostic should succeed (already zeroed)
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY);
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, ENTRY, SENTINEL) == VF2_OK);
    // the bridge expects ip == ENTRY and local_frame_depth >=1 after enter
    // enter_procedure sets ip to ENTRY and depth 1, so ok
    vf2_status st = vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report);
    CHECK(st == VF2_OK);
    CHECK(cpu.ip == EXIT || cpu.ip == SENTINEL || report.exit_address == EXIT);
    vf2_model2a_shutdown(&machine);
    return report;
}

int main(void) {
    // measured reference via vf2probe from tex_counter_boundary.vf2snap
    // in-range arg0 = 0x10, status 0
    struct {uint32_t c0,c1,c2; uint64_t instr; uint64_t calls;} cases[] = {
        {0,0,0, 12, 0}, {0,0,1, 55, 3}, {0,0,2, 14, 0},
        {0,1,0, 91, 4}, {0,1,1, 134, 7}, {0,1,2, 93, 4},
        {0,2,0, 14, 0}, {0,2,1, 57, 3}, {0,2,2, 16, 0},
        {1,0,0, 91, 4}, {1,0,1, 134, 7}, {1,0,2, 93, 4},
        {1,1,0, 142, 8}, {1,1,1, 185, 11}, {1,1,2, 144, 8},
        {1,2,0, 93, 4}, {1,2,1, 136, 7}, {1,2,2, 95, 4},
        {2,0,0, 14, 0}, {2,0,1, 57, 3}, {2,0,2, 16, 0},
        {2,1,0, 93, 4}, {2,1,1, 136, 7}, {2,1,2, 95, 4},
        {2,2,0, 16, 0}, {2,2,1, 59, 3}, {2,2,2, 18, 0},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        vf2_hybrid_bridge_report r = run_counter_case(cases[i].c0, cases[i].c1, cases[i].c2, 0, 0x10, 0x10, 0x10);
        if (r.recovered_instruction_count != cases[i].instr) {
            fprintf(stderr, "case %u,%u,%u instr %llu != expected %llu\n",
                (unsigned)cases[i].c0, (unsigned)cases[i].c1, (unsigned)cases[i].c2,
                (unsigned long long)r.recovered_instruction_count,
                (unsigned long long)cases[i].instr);
            ++failures;
        }
        if (r.recovered_procedure_calls != cases[i].calls) {
            fprintf(stderr, "case %u,%u,%u calls %llu != expected %llu\n",
                (unsigned)cases[i].c0, (unsigned)cases[i].c1, (unsigned)cases[i].c2,
                (unsigned long long)r.recovered_procedure_calls,
                (unsigned long long)cases[i].calls);
            ++failures;
        }
        CHECK(r.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
        CHECK(r.entry_address == ENTRY);
        CHECK(r.exit_address == EXIT);
    }
    // status_word invariance: c2==1 should be same for 0,1,0xFFFF
    for (uint16_t sw = 0; sw <= 2; ++sw) {
        uint16_t v = sw == 2 ? 0xFFFF : sw;
        vf2_hybrid_bridge_report r = run_counter_case(0,0,1, v, 0x10, 0x10, 0x10);
        CHECK(r.recovered_instruction_count == 55);
        CHECK(r.recovered_procedure_calls == 3);
    }
    // out-of-range counter0: should take diagnostic path (adds 160 per publish)
    // we seeded arg0_c0 = 0x57 (>0x56) -> expect larger instr (375 for c0=1 case)
    {
        vf2_hybrid_bridge_report r = run_counter_case(1,0,0, 0, 0x57, 0x10, 0x10);
        // in-range 1,0,0 was 91; out-of-range adds diagnostic: measured 375 via synthetic
        if (r.recovered_instruction_count != 375) {
            fprintf(stderr, "out-of-range c0=1 instr %llu != 375\n", (unsigned long long)r.recovered_instruction_count);
            ++failures;
        }
    }
    if (failures) {
        fprintf(stderr, "%d texture-counter-update test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("texture-counter-update tests passed");
    return EXIT_SUCCESS;
}
