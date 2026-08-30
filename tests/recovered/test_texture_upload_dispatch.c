#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

static int failures = 0;

#define CHECK(expr)                                           \
    do {                                                      \
        if (!(expr)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++failures;                                       \
        }                                                     \
    } while (0)

#define DISPATCH_ENTRY UINT32_C(0x0004ba80)
#define PENDING0_ADDR UINT32_C(0x005502a8)
#define PENDING1_ADDR UINT32_C(0x005502b0)
#define PENDING2_ADDR UINT32_C(0x005502b8)
#define SENTINEL UINT32_C(0x00012340)

static void seed_u16(vf2_model2a *m, uint32_t addr, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xff), (uint8_t)(v >> 8)};
    CHECK(vf2_model2a_write(m, addr, b, sizeof(b)) == VF2_OK);
}

static vf2_status run_dispatch(int arg0, int arg1, vf2_hybrid_bridge_report *out_report) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;
    memset(&report, 0, sizeof(report));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    {
        size_t sz = 0x00400000;
        uint8_t *md = (uint8_t*)calloc(sz, 1);
        CHECK(md != NULL);
        CHECK(vf2_model2a_take_main_data(&machine, md, sz) == VF2_OK);
    }
    seed_u16(&machine, PENDING0_ADDR, 0);
    seed_u16(&machine, PENDING0_ADDR + 2, 0);
    seed_u16(&machine, PENDING0_ADDR + 4, 0);
    seed_u16(&machine, PENDING1_ADDR, 0);
    seed_u16(&machine, PENDING1_ADDR + 2, 0);
    seed_u16(&machine, PENDING1_ADDR + 4, 0);
    seed_u16(&machine, PENDING2_ADDR, 1);
    seed_u16(&machine, PENDING2_ADDR + 2, (uint16_t)arg0);
    seed_u16(&machine, PENDING2_ADDR + 4, (uint16_t)arg1);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, DISPATCH_ENTRY);
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, DISPATCH_ENTRY, SENTINEL) == VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, DISPATCH_ENTRY, DISPATCH_ENTRY) == VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, DISPATCH_ENTRY, DISPATCH_ENTRY) == VF2_OK);
    vf2_status st = vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report);
    if (out_report) *out_report = report;
    vf2_model2a_shutdown(&machine);
    return st;
}

int main(void) {
    struct {int a0; int a1; int should_ok;} cases[] = {
        {0, 0xa, 0},
        {0, 0xb, 1},
        {0, 0xc, 0},
        {1, 0xa, 0},
        {1, 0xb, 0},
        {1, 0xc, 0},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        vf2_hybrid_bridge_report r;
        vf2_status st = run_dispatch(cases[i].a0, cases[i].a1, &r);
        if (cases[i].should_ok) {
            if (st != VF2_OK) {
                fprintf(stderr, "case a0=%d a1=0x%x expected OK got %d\n", cases[i].a0, cases[i].a1, (int)st);
                ++failures;
            } else {
                CHECK(r.kind == VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH);
                CHECK(r.entry_address == DISPATCH_ENTRY);
                CHECK(r.exit_address == UINT32_C(0x0004bb14));
                CHECK(r.recovered_instruction_count == UINT64_C(2035));
            }
        } else {
            if (st != VF2_ERROR_UNSUPPORTED) {
                fprintf(stderr, "case a0=%d a1=0x%x expected UNSUPPORTED got %d\n", cases[i].a0, cases[i].a1, (int)st);
                ++failures;
            }
        }
    }
    if (failures) {
        fprintf(stderr, "%d texture-upload-dispatch test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("texture-upload-dispatch tests passed");
    return EXIT_SUCCESS;
}
