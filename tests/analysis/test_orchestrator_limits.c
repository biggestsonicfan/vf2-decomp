#include <stdint.h>
#include <stdio.h>

#include "vf2/analysis/orchestrator_limits.h"
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

static void test_default_branch_class(void)
{
    static const uint8_t accepted_modes[] = {
        0u, 1u, 4u, 5u, 8u, 10u, 11u, 16u, 31u
    };
    static const uint8_t unsupported_modes[] = {
        2u, 3u, 6u, 7u, 9u, 12u, 13u, 14u, 15u, 32u, 255u
    };
    size_t index = 0u;

    for (index = 0u;
         index < sizeof(accepted_modes) / sizeof(accepted_modes[0]);
         ++index) {
        uint32_t lower = 0u;
        uint32_t upper = 0u;
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u,
                accepted_modes[index],
                &lower,
                &upper
            ) == VF2_OK
        );
        CHECK(lower == UINT32_C(0x00003e80));
        CHECK(upper == UINT32_C(0x00004e20));
    }

    for (index = 0u;
         index < sizeof(unsupported_modes) / sizeof(unsupported_modes[0]);
         ++index) {
        uint32_t lower = UINT32_C(0x11111111);
        uint32_t upper = UINT32_C(0x22222222);
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u,
                unsupported_modes[index],
                &lower,
                &upper
            ) == VF2_ERROR_UNSUPPORTED
        );
        CHECK(lower == UINT32_C(0x11111111));
        CHECK(upper == UINT32_C(0x22222222));
    }

    {
        uint32_t lower = 0u;
        uint32_t upper = 0u;
        CHECK(
            vf2_orchestrator_select_default_limits(
                UINT32_C(1) << 16u,
                0u,
                &lower,
                &upper
            ) == VF2_ERROR_UNSUPPORTED
        );
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u, 0u, NULL, &upper
            ) == VF2_ERROR_INVALID_ARGUMENT
        );
        CHECK(
            vf2_orchestrator_select_default_limits(
                0u, 0u, &lower, NULL
            ) == VF2_ERROR_INVALID_ARGUMENT
        );
    }
}

static void test_machine_application(void)
{
    vf2_model2a machine;
    vf2_orchestrator_limits_report report;
    uint32_t value = 0u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_RUNTIME_FLAGS, 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &mode,
            sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK
    );
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LIMITS_ENTRY);
    CHECK(report.runtime_flags == 0u);
    CHECK(report.display_mode == 0u);
    CHECK(report.lower_limit == UINT32_C(0x00003e80));
    CHECK(report.upper_limit == UINT32_C(0x00004e20));
    CHECK(report.interpreted_instruction_equivalent == UINT64_C(22));
    CHECK(report.bytes_written == 8u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_LOW, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x00003e80));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x00004e20));

    mode = 9u;
    CHECK(
        vf2_model2a_write(
            &machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &mode,
            sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            VF2_ORCHESTRATOR_LIMIT_LOW,
            UINT32_C(0x11111111)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            VF2_ORCHESTRATOR_LIMIT_HIGH,
            UINT32_C(0x22222222)
        ) == VF2_OK
    );
    CHECK(
        vf2_orchestrator_apply_default_limits(&machine, NULL) ==
        VF2_ERROR_UNSUPPORTED
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_LOW, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x11111111));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x22222222));

    vf2_model2a_shutdown(&machine);
}

static void test_orchestrator_prefix_bridges(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t stack_start = VF2_WORK_RAM_BASE + UINT32_C(0x2000);
    uint32_t expected[28];
    uint32_t value = 0u;
    uint32_t index = 0u;
    uint8_t frame_state = 0u;
    uint8_t latch = UINT8_C(0xff);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x1000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu,
            UINT32_C(0x0004bb18),
            UINT32_C(0x00001004)
        ) == VF2_OK
    );
    cpu.registers[1] = stack_start;
    for (index = 3u; index <= 30u; ++index) {
        expected[index - 3u] = UINT32_C(0x31000000) + index;
        cpu.registers[index] = expected[index - 3u];
    }

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(
        report.kind ==
        VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL
    );
    CHECK(report.entry_address == UINT32_C(0x0004bb18));
    CHECK(report.exit_address == UINT32_C(0x0004bcd4));
    CHECK(report.iterations == UINT64_C(28));
    CHECK(report.bytes_written == 112u);
    CHECK(report.recovered_instruction_count == UINT64_C(21));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004bcd4));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(21));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    for (index = 0u; index < 28u; ++index) {
        CHECK(
            vf2_model2a_read_u32(
                &machine,
                stack_start + index * UINT32_C(4),
                &value
            ) == VF2_OK
        );
        CHECK(value == expected[index]);
    }

    CHECK(
        vf2_model2a_write(
            &machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE - UINT32_C(0x2b),
            &frame_state,
            sizeof(frame_state)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x0050006d),
            &latch,
            sizeof(latch)
        ) == VF2_OK
    );
    report = (vf2_hybrid_bridge_report){0};
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(
        report.kind == VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL
    );
    CHECK(report.entry_address == UINT32_C(0x0004bcd4));
    CHECK(report.exit_address == UINT32_C(0x0004bfe0));
    CHECK(report.bytes_written == 1u);
    CHECK(report.recovered_instruction_count == UINT64_C(5));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004bfe0));
    CHECK(cpu.local_frame_depth == 3u);
    CHECK(cpu.maximum_local_frame_depth == 3u);
    CHECK(cpu.executed_instructions == UINT64_C(26));
    CHECK(cpu.procedure_calls == UINT64_C(3));
    CHECK(cpu.registers[16] == 0u);
    CHECK(
        vf2_model2a_read(
            &machine,
            UINT32_C(0x0050006d),
            &latch,
            sizeof(latch)
        ) == VF2_OK
    );
    CHECK(latch == 0u);

    vf2_model2a_shutdown(&machine);
}

static void test_hybrid_bridge_poststate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t value = 0u;
    uint32_t index = 0u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_RUNTIME_FLAGS, 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &mode,
            sizeof(mode)
        ) == VF2_OK
    );

    vf2_i960_cpu_reset(
        &cpu, 0u, 0u, UINT32_C(0x0004bcfc)
    );
    for (index = 0u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        cpu.registers[index] = UINT32_C(0xa5000000) + index;
    }
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu,
            VF2_ORCHESTRATOR_LIMITS_ENTRY,
            UINT32_C(0x0004bd00)
        ) == VF2_OK
    );
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );

    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LIMITS_ENTRY);
    CHECK(report.exit_address == UINT32_C(0x0004bd00));
    CHECK(report.changed_values == UINT64_C(2));
    CHECK(report.bytes_written == 8u);
    CHECK(report.recovered_instruction_count == UINT64_C(22));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bd00));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.maximum_local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(22));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[10] == UINT32_C(0xa500000a));
    CHECK(cpu.registers[11] == UINT32_C(0xa500000b));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_LOW, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x00003e80));
    CHECK(
        vf2_model2a_read_u32(
            &machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x00004e20));

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_default_branch_class();
    test_machine_application();
    test_orchestrator_prefix_bridges();
    test_hybrid_bridge_poststate();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-limit test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-limit tests passed\n");
    return 0;
}
