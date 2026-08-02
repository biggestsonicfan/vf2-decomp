#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/native_runtime.h"

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

static void test_initialize_and_names(void)
{
    vf2_native_runtime_state state;

    memset(&state, 0xff, sizeof(state));
    CHECK(vf2_native_runtime_initialize(NULL, 4u) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_native_runtime_initialize(&state, 0u) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(state.blocks_executed == 0u);
    CHECK(state.frame_wait.visits_before_interrupt == 0u);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    CHECK(state.frame_wait.visits_before_interrupt == 4u);
    CHECK(state.blocks_executed == 0u);
    CHECK(
        strcmp(
            vf2_native_runtime_step_kind_name(
                VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER
            ),
            "second-scheduler"
        ) == 0
    );
    CHECK(
        strcmp(
            vf2_native_runtime_step_kind_name(VF2_NATIVE_RUNTIME_STEP_TASK),
            "task"
        ) == 0
    );
}

static void test_zero_length_run(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x12345678));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0xff, sizeof(report));
    CHECK(
        vf2_native_runtime_run_until(
            &machine,
            &cpu,
            &state,
            UINT32_C(0x12345678),
            0u,
            &report
        ) == VF2_OK
    );
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_executed == 0u);
    CHECK(report.start_address == UINT32_C(0x12345678));
    CHECK(report.final_address == UINT32_C(0x12345678));
    CHECK(state.blocks_executed == 0u);
    vf2_model2a_shutdown(&machine);
}

static void test_single_bridge_run(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    uint8_t enabled = 1u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x00500171),
            &enabled,
            sizeof(enabled)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x0050002b),
            &mode,
            sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x0059c318),
            0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x0059c31c),
            0u
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0006dcb8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(
        vf2_native_runtime_run_until(
            &machine,
            &cpu,
            &state,
            UINT32_C(0x00001004),
            1u,
            &report
        ) == VF2_OK
    );
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_executed == 1u);
    CHECK(report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(
        report.last_bridge_kind ==
        VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC
    );
    CHECK(report.recovered_instruction_count == UINT64_C(75));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(state.blocks_executed == 1u);
    CHECK(state.recovered_instruction_count == UINT64_C(75));
    CHECK(cpu.ip == UINT32_C(0x00001004));

    vf2_model2a_shutdown(&machine);
}

static void test_second_game_info_task_run(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    const uint32_t registry = UINT32_C(0x00515200);
    const uint32_t fighter0 = UINT32_C(0x00502000);
    const uint32_t fighter1 = UINT32_C(0x00503000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00500804),
            fighter0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00500808),
            fighter1
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, fighter0, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, 0u) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00508000),
            UINT32_C(1) << 5u
        ) == VF2_OK
    );

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00010d54));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu.registers[29] = registry;
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu,
            UINT32_C(0x0001645c),
            UINT32_C(0x00010dcc)
        ) == VF2_OK
    );
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(
        vf2_native_runtime_run_until(
            &machine,
            &cpu,
            &state,
            UINT32_C(0x00010dcc),
            1u,
            &report
        ) == VF2_OK
    );
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_executed == 1u);
    CHECK(report.task_bodies_executed == 1u);
    CHECK(report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.last_task_kind == VF2_HYBRID_TASK_GAME_INFO);
    CHECK(report.recovered_instruction_count == UINT64_C(19));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(state.blocks_executed == 1u);
    CHECK(state.task_bodies_executed == 1u);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.registers[23] == fighter1);
    CHECK(cpu.registers[24] == fighter0);

    vf2_model2a_shutdown(&machine);
}

static void test_budget_and_unsupported_are_explicit(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    vf2_native_runtime_step_report step_report;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0xdeadbeef));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&step_report, 0xff, sizeof(step_report));
    CHECK(
        vf2_native_runtime_step(
            &machine,
            &cpu,
            &state,
            &step_report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(state.blocks_executed == 0u);
    CHECK(step_report.entry_address == UINT32_C(0xdeadbeef));
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_NONE);

    memset(&report, 0, sizeof(report));
    CHECK(
        vf2_native_runtime_run_until(
            &machine,
            &cpu,
            &state,
            UINT32_C(0xcafebabe),
            0u,
            &report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(report.reached_stop == 0);
    CHECK(report.blocks_executed == 0u);
    CHECK(report.final_address == UINT32_C(0xdeadbeef));
    CHECK(state.blocks_executed == 0u);

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_initialize_and_names();
    test_zero_length_run();
    test_single_bridge_run();
    test_second_game_info_task_run();
    test_budget_and_unsupported_are_explicit();

    if (failures != 0) {
        fprintf(stderr, "%d native-runtime test(s) failed\n", failures);
        return 1;
    }
    printf("native-runtime tests passed\n");
    return 0;
}
