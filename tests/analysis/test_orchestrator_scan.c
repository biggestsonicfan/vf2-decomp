#include <stdint.h>
#include <stdio.h>

#include "vf2/analysis/orchestrator_scan.h"

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

static void write_record_count(
    vf2_model2a *machine,
    size_t record_index,
    uint16_t active_count
)
{
    const uint32_t address =
        VF2_ORCHESTRATOR_RECORD_START +
        (uint32_t)record_index * VF2_ORCHESTRATOR_RECORD_STRIDE +
        VF2_ORCHESTRATOR_RECORD_ACTIVE_OFFSET;
    const uint8_t bytes[2] = {
        (uint8_t)active_count,
        (uint8_t)(active_count >> 8u)
    };

    CHECK(
        vf2_model2a_write(machine, address, bytes, sizeof(bytes)) ==
        VF2_OK
    );
}

static void enter_scan(vf2_i960_cpu *cpu)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu,
            VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY,
            UINT32_C(0x00001004)
        ) == VF2_OK
    );
}

static void test_complete_inactive_scan(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_orchestrator_scan_report report = {0};
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < VF2_ORCHESTRATOR_RECORD_COUNT; ++index) {
        write_record_count(&machine, index, 0u);
    }
    enter_scan(&cpu);
    cpu.compare_result = VF2_I960_COMPARE_LESS;
    cpu.arithmetic_control = UINT32_C(0x3f001004);

    CHECK(
        vf2_orchestrator_scan_inactive_records(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.entry_address == VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);
    CHECK(report.exit_address == VF2_ORCHESTRATOR_RECORD_SCAN_EXIT);
    CHECK(report.first_record_address == VF2_ORCHESTRATOR_RECORD_START);
    CHECK(report.end_record_address == VF2_ORCHESTRATOR_RECORD_END);
    CHECK(report.records_scanned == VF2_ORCHESTRATOR_RECORD_COUNT);
    CHECK(report.recovered_instruction_count == UINT64_C(43));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == VF2_ORCHESTRATOR_RECORD_SCAN_EXIT);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(43));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.registers[5] == VF2_ORCHESTRATOR_RECORD_END);
    CHECK(cpu.registers[6] == VF2_ORCHESTRATOR_RECORD_END);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001004));

    vf2_model2a_shutdown(&machine);
}

static void test_active_record_is_rejected(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_orchestrator_scan_report report = {0};
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < VF2_ORCHESTRATOR_RECORD_COUNT; ++index) {
        write_record_count(&machine, index, index == 4u ? 1u : 0u);
    }
    enter_scan(&cpu);

    CHECK(
        vf2_orchestrator_scan_inactive_records(
            &machine, &cpu, &report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(cpu.ip == VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);
    CHECK(cpu.executed_instructions == UINT64_C(0));
    CHECK(cpu.registers[3] == UINT32_C(1));
    CHECK(
        cpu.registers[5] ==
        VF2_ORCHESTRATOR_RECORD_START + UINT32_C(4) *
            VF2_ORCHESTRATOR_RECORD_STRIDE
    );

    vf2_model2a_shutdown(&machine);
}

static void test_invalid_entry_state(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0004bd28));
    CHECK(
        vf2_orchestrator_scan_inactive_records(
            &machine, &cpu, NULL
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_orchestrator_scan_inactive_records(NULL, &cpu, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_orchestrator_scan_inactive_records(&machine, NULL, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_complete_inactive_scan();
    test_active_record_is_rejected();
    test_invalid_entry_state();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-scan test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-scan tests passed\n");
    return 0;
}
