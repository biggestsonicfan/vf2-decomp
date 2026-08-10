#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void enter_parent(vf2_i960_cpu *cpu, uint32_t target)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu,
            target,
            UINT32_C(0x00001004)
        ) == VF2_OK
    );
}

static void set_greater(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control = UINT32_C(0x3f001001);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
}

static void test_inactive_palette_upload_preserves_condition(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00546000),
            0u
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x00002de4));
    set_greater(&cpu);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine,
            &cpu,
            &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD);
    CHECK(report.entry_address == UINT32_C(0x00002de4));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001001));

    vf2_model2a_shutdown(&machine);
}

static void test_active_palette_upload_all_pages(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;
    uint8_t *scratch = NULL;
    uint32_t first = 0u;
    uint32_t last = 0u;
    const size_t scratch_size = (size_t)UINT32_C(28 * 288);

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    scratch = (uint8_t *)calloc(scratch_size, 1u);
    CHECK(scratch != NULL);
    if (scratch == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    scratch[0] = UINT8_C(0x34);
    scratch[1] = UINT8_C(0x12);
    scratch[2] = UINT8_C(0x78);
    scratch[3] = UINT8_C(0x56);
    scratch[4] = UINT8_C(0xbc);
    scratch[5] = UINT8_C(0x9a);
    scratch[scratch_size - 6u] = UINT8_C(0xef);
    scratch[scratch_size - 5u] = UINT8_C(0xbe);
    scratch[scratch_size - 4u] = UINT8_C(0xad);
    scratch[scratch_size - 3u] = UINT8_C(0xde);
    scratch[scratch_size - 2u] = UINT8_C(0x11);
    scratch[scratch_size - 1u] = UINT8_C(0x22);
    CHECK(
        vf2_model2a_write(
            &machine, UINT32_C(0x00546008), scratch, scratch_size
        ) == VF2_OK
    );
    free(scratch);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00546000), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00546004), UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f0000c), UINT32_C(0x000fff00)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00002de4));
    set_greater(&cpu);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD);
    CHECK(report.entry_address == UINT32_C(0x00002de4));
    CHECK(report.rows == UINT64_C(28));
    CHECK(report.iterations == UINT64_C(28 * 48));
    CHECK(report.bytes_written == (size_t)UINT32_C(28 * 48 * 6));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00546000), &first) == VF2_OK);
    CHECK(first == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00546004), &last) == VF2_OK);
    CHECK(last == UINT16_C(28));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x01810000), &first) == VF2_OK);
    CHECK((first & UINT16_C(0xffff)) == UINT16_C(0x1234));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0181365e), &last) == VF2_OK);
    CHECK((last & UINT16_C(0xffff)) == UINT16_C(0xbeef));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0181b65e), &last) == VF2_OK);
    CHECK((last & UINT16_C(0xffff)) == UINT16_C(0x2211));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    vf2_model2a_shutdown(&machine);
}

static void test_frame_timer_zero_modulo_counter_path(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;
    uint32_t stored_minimum = UINT32_C(0xffffffff);
    uint8_t zero = 0u;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00f00008),
            UINT32_C(0x000fffff)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00f0000c),
            UINT32_C(0x0007a120)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x00500000),
            &zero,
            sizeof(zero)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00500020),
            UINT32_C(32)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00500160),
            UINT32_C(0x12345678)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x0050006d),
            &zero,
            sizeof(zero)
        ) == VF2_OK
    );

    enter_parent(&cpu, UINT32_C(0x00010f08));
    set_greater(&cpu);
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine,
            &cpu,
            &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX);
    CHECK(report.entry_address == UINT32_C(0x00010f08));
    CHECK(report.exit_address == UINT32_C(0x00010f90));
    CHECK(report.recovered_instruction_count == UINT64_C(21));
    CHECK(cpu.ip == UINT32_C(0x00010f90));
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001001));
    CHECK(
        vf2_model2a_read_u32(
            &machine,
            UINT32_C(0x00500160),
            &stored_minimum
        ) == VF2_OK
    );
    CHECK(stored_minimum == cpu.registers[5]);

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_inactive_palette_upload_preserves_condition();
    test_active_palette_upload_all_pages();
    test_frame_timer_zero_modulo_counter_path();

    if (failures != 0) {
        fprintf(stderr, "%d endurance regression test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("endurance regression tests passed");
    return EXIT_SUCCESS;
}
