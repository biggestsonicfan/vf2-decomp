#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static void test_multi_frame_run(void)
{
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report step_report;
    uint8_t flag = 0u;

    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    CHECK(rom != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (machine.work_ram == NULL) {
        free(rom);
        return;
    }

    CHECK(
        vf2_model2a_attach_main_rom(
            &machine,
            rom,
            VF2_MAIN_ROM_SIZE
        ) == VF2_OK
    );

    /* Point Processor Control Block and Interrupt Table and Stack Pointer */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff410) + 20u, UINT32_C(0x005ff000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff410) + 24u, UINT32_C(0x005ff500)) == VF2_OK);
    /* Point vector 12 to VF2_NATIVE_INTERRUPT_RETURN_ENTRY = 0x00000d20 directly */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff000) + 36u + 16u, UINT32_C(0x00000d20)) == VF2_OK);

    /* Initialize CPU at frame wait entry */
    vf2_i960_cpu_reset(&cpu, 0u, UINT32_C(0x005ff410), UINT32_C(0x00010f90));
    cpu.registers[1] = UINT32_C(0x00501000);
    cpu.registers[31] = UINT32_C(0x00500000);

    /* Write 0 to frame counter */
    flag = 0u;
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x00500000),
            &flag,
            sizeof(flag)
        ) == VF2_OK
    );

    /* Initialize native runtime with 2 visits before interrupt */
    CHECK(vf2_native_runtime_initialize(&state, 2u) == VF2_OK);

    /* Step 1: Execute frame wait poll, visits = 1, continues */
    memset(&step_report, 0, sizeof(step_report));
    CHECK(
        vf2_native_runtime_step(
            &machine,
            &cpu,
            &state,
            &step_report
        ) == VF2_OK
    );
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(state.frame_wait_phases == 1u);
    CHECK(state.frame_wait.visits == 0u); // reset back to 0 because 2 >= 2 and it raised interrupt!
    CHECK(state.frame_wait.interrupts_injected == 1u);
    CHECK(cpu.ip == UINT32_C(0x00000d20)); // Interrupted and jumped to the vector 12 handler
    CHECK(cpu.local_frame_depth == 1u);

    /* Change frame byte value at 0x500000 to exit the wait on return */
    flag = 1u;
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x00500000),
            &flag,
            sizeof(flag)
        ) == VF2_OK
    );

    /* Step 2: Execute vector 12 interrupt handler and return from interrupt */
    memset(&step_report, 0, sizeof(step_report));
    CHECK(
        vf2_native_runtime_step(
            &machine,
            &cpu,
            &state,
            &step_report
        ) == VF2_OK
    );
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(state.frame_wait_phases == 2u);
    CHECK(state.frame_wait.visits == 1u);
    CHECK(cpu.ip == UINT32_C(0x00010fa4)); // Succeeded interrupt return and frame exit!
    CHECK(cpu.local_frame_depth == 0u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_repeated_scheduler_entry_dispatches_recovery(void)
{
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report step_report;
    vf2_native_runtime_run_report run_report;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (machine.work_ram == NULL) {
        free(rom);
        return;
    }
    /* The recovered scheduler scan reads task_count from a low address inside
     * the main ROM window; attach a blank ROM so the read returns 0 instead of
     * VF2_ERROR_OUT_OF_BOUNDS. */
    CHECK(
        vf2_model2a_attach_main_rom(
            &machine,
            rom,
            VF2_MAIN_ROM_SIZE
        ) == VF2_OK
    );

    /* Stand the CPU exactly at the recovered main-loop scheduler call site
     * (0x0000a010), matching the architectural preconditions required by
     * vf2_hybrid_second_scheduler_enter for the second sweep. The state
     * already records one accepted second-sweep entry, simulating the
     * end-of-frame re-hit of the same call site that should launch a third
     * sweep. The recovered scheduler entry is now generic across sweeps --
     * reference evidence (observe-third-sweep) confirms the architectural
     * preconditions are met on every sweep -- so the runtime must dispatch
     * the actual recovery rather than short-circuiting. With task_count == 0
     * the inner enter rejects via its own preconditions. */
    vf2_i960_cpu_reset(
        &cpu,
        0u,
        UINT32_C(0x005ff410),
        UINT32_C(0x0000a010)
    );
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    state.scheduler_entries = 1u;

    memset(&step_report, 0xff, sizeof(step_report));
    CHECK(
        vf2_native_runtime_step(
            &machine,
            &cpu,
            &state,
            &step_report
        ) == VF2_ERROR_UNSUPPORTED
    );
    /* The runtime did not short-circuit with a distinct third-scheduler step
     * kind; it forwarded the call to vf2_hybrid_second_scheduler_enter, which
     * rejected the unseeded scheduler state via its own preconditions. */
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_NONE);
    CHECK(step_report.entry_address == UINT32_C(0x0000a010));
    CHECK(step_report.recovered_instruction_count == UINT64_C(0));
    /* Unsupported steps must not advance any recovered accounting. */
    CHECK(state.blocks_executed == 0u);
    CHECK(state.recovered_instruction_count == UINT64_C(0));
    CHECK(state.scheduler_entries == 1u);

    /* The same observation must surface through run_until's report. */
    memset(&run_report, 0xff, sizeof(run_report));
    CHECK(
        vf2_native_runtime_run_until(
            &machine,
            &cpu,
            &state,
            UINT32_C(0x00000000),
            4u,
            &run_report
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(run_report.reached_stop == 0);
    CHECK(run_report.blocks_executed == 0u);
    CHECK(run_report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_NONE);
    CHECK(run_report.final_address == UINT32_C(0x0000a010));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

int main(void)
{
    test_initialize_and_names();
    test_zero_length_run();
    test_single_bridge_run();
    test_second_game_info_task_run();
    test_budget_and_unsupported_are_explicit();
    test_multi_frame_run();
    test_repeated_scheduler_entry_dispatches_recovery();

    if (failures != 0) {
        fprintf(stderr, "%d native-runtime test(s) failed\n", failures);
        return 1;
    }
    printf("native-runtime tests passed\n");
    return 0;
}
