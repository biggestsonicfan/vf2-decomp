#include <stdint.h>
#include <stdio.h>

#include "vf2/analysis/orchestrator_gates.h"

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
            cpu, target, UINT32_C(0x00001004)
        ) == VF2_OK
    );
}

static void test_child_gate(
    uint32_t entry,
    uint32_t expected_target,
    uint32_t expected_return,
    vf2_orchestrator_gate_kind expected_kind
)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_orchestrator_gate_report report = {0};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_CHILD_STATE, 0u
        ) == VF2_OK
    );
    enter_parent(&cpu, entry);

    CHECK(
        vf2_orchestrator_enter_zero_child_gate(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == expected_kind);
    CHECK(report.entry_address == entry);
    CHECK(report.exit_address == expected_target);
    CHECK(report.child_state == 0u);
    CHECK(report.call_target == expected_target);
    CHECK(report.return_address == expected_return);
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == expected_target);
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(3));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);

    vf2_model2a_shutdown(&machine);
}

static void test_loop_tail_gate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_orchestrator_gate_report report = {0};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_CHILD_STATE, 0u
        ) == VF2_OK
    );
    enter_parent(&cpu, VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);

    CHECK(
        vf2_orchestrator_apply_zero_loop_gate(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_ORCHESTRATOR_GATE_LOOP_TAIL);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);
    CHECK(report.exit_address == VF2_ORCHESTRATOR_LOOP_GATE_EXIT);
    CHECK(report.child_state == 0u);
    CHECK(report.call_target == 0u);
    CHECK(report.return_address == 0u);
    CHECK(report.recovered_instruction_count == UINT64_C(2));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == VF2_ORCHESTRATOR_LOOP_GATE_EXIT);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(2));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);

    vf2_model2a_shutdown(&machine);
}

static void test_nonzero_state_is_rejected(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            VF2_ORCHESTRATOR_CHILD_STATE,
            UINT32_C(0x12345678)
        ) == VF2_OK
    );

    enter_parent(&cpu, VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY);
    CHECK(
        vf2_orchestrator_enter_zero_child_gate(
            &machine, &cpu, NULL
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(cpu.ip == VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(0));
    CHECK(
        cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x12345678)
    );

    enter_parent(&cpu, VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);
    CHECK(
        vf2_orchestrator_apply_zero_loop_gate(
            &machine, &cpu, NULL
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(cpu.ip == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);
    CHECK(cpu.executed_instructions == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_invalid_inputs(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0004bebc));

    CHECK(
        vf2_orchestrator_enter_zero_child_gate(NULL, &cpu, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_orchestrator_enter_zero_child_gate(&machine, NULL, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_orchestrator_enter_zero_child_gate(
            &machine, &cpu, NULL
        ) == VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_orchestrator_apply_zero_loop_gate(
            &machine, &cpu, NULL
        ) == VF2_ERROR_UNSUPPORTED
    );

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_child_gate(
        VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY,
        VF2_ORCHESTRATOR_CHILD_GATE_A_TARGET,
        VF2_ORCHESTRATOR_CHILD_GATE_A_RETURN,
        VF2_ORCHESTRATOR_GATE_CHILD_A
    );
    test_child_gate(
        VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY,
        VF2_ORCHESTRATOR_CHILD_GATE_B_TARGET,
        VF2_ORCHESTRATOR_CHILD_GATE_B_RETURN,
        VF2_ORCHESTRATOR_GATE_CHILD_B
    );
    test_loop_tail_gate();
    test_nonzero_state_is_rejected();
    test_invalid_inputs();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-gate test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-gate tests passed\n");
    return 0;
}
