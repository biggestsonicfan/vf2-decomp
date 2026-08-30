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

static void test_select_full_sweep_512(void)
{
    uint32_t runtime_flags_values[2] = {0u, UINT32_C(1) << 16u};
    size_t rt_index = 0u;
    uint32_t mode = 0u;
    size_t total = 0u;
    size_t ok_count = 0u;
    size_t unsup_count = 0u;

    for (rt_index = 0u; rt_index < 2u; ++rt_index) {
        uint32_t runtime_flags = runtime_flags_values[rt_index];
        for (mode = 0u; mode < 256u; ++mode) {
            uint8_t display_mode = (uint8_t)mode;
            uint32_t lower = UINT32_C(0xdeadbeef);
            uint32_t upper = UINT32_C(0xcafebabe);
            uint32_t expect_lower = 0u;
            uint32_t expect_upper = 0u;
            vf2_status expect_status = VF2_OK;
            vf2_status status = VF2_OK;

            /* Expected mapping derived from synthetic snapshot sweep at 0x4bfe0
             * via vf2probe --rom-dir D:/ia/vf2-decomp/roms/vf2 --snapshot snap_orch.vf2snap --until 0x4c11c --read-u32.
             * Extra fields 0x50064 and 0x50031 are zero in the sweep. */
            if ((runtime_flags & (UINT32_C(1) << 16u)) != 0u) {
                expect_lower = 0u;
                expect_upper = UINT32_C(0x00004e20);
                expect_status = VF2_OK;
            } else if (display_mode == UINT8_C(12) || display_mode == UINT8_C(13)) {
                expect_lower = 0u;
                expect_upper = UINT32_C(0x00004e20);
                expect_status = VF2_OK;
            } else if ((display_mode % UINT8_C(32)) == UINT8_C(6) || (display_mode % UINT8_C(32)) == UINT8_C(7)) {
                expect_lower = UINT32_C(0x000012a8);
                expect_upper = UINT32_C(0x00004330);
                expect_status = VF2_OK;
            } else if ((display_mode % UINT8_C(32)) == UINT8_C(14) || (display_mode % UINT8_C(32)) == UINT8_C(15)) {
                expect_lower = UINT32_C(0x000032c8);
                expect_upper = UINT32_C(0x00004e20);
                expect_status = VF2_OK;
            } else if ((display_mode % UINT8_C(32)) == UINT8_C(2) || (display_mode % UINT8_C(32)) == UINT8_C(3)) {
                expect_status = VF2_ERROR_UNSUPPORTED;
            } else if (display_mode == UINT8_C(9)) {
                /* With field_50064=0 and field_50031=0 (<8), reference at 0x4bfe0 yields 0x4330/0. */
                expect_lower = UINT32_C(0x00004330);
                expect_upper = 0u;
                expect_status = VF2_OK;
            } else {
                expect_lower = UINT32_C(0x00003e80);
                expect_upper = UINT32_C(0x00004e20);
                expect_status = VF2_OK;
            }

            status = vf2_orchestrator_select_limits(
                runtime_flags, display_mode, 0u, 0u, &lower, &upper
            );
            CHECK(status == expect_status);
            if (expect_status == VF2_OK) {
                CHECK(lower == expect_lower);
                CHECK(upper == expect_upper);
                ++ok_count;
            } else {
                CHECK(lower == UINT32_C(0xdeadbeef));
                CHECK(upper == UINT32_C(0xcafebabe));
                ++unsup_count;
            }
            /* Also check the backwards-compatible wrapper. */
            lower = UINT32_C(0xdeadbeef);
            upper = UINT32_C(0xcafebabe);
            status = vf2_orchestrator_select_default_limits(runtime_flags, display_mode, &lower, &upper);
            CHECK(status == expect_status);
            if (expect_status == VF2_OK) {
                CHECK(lower == expect_lower);
                CHECK(upper == expect_upper);
            }
            ++total;
        }
    }
    CHECK(total == 512u);
    /* With extras zero: 16 skip cases remain unsupported, 496 supported. */
    CHECK(unsup_count == 16u);
    CHECK(ok_count == 496u);

    /* Exhaustive check for mode 9 extra field behaviour. */
    {
        uint32_t lower = 0u, upper = 0u;
        CHECK(vf2_orchestrator_select_limits(0u, 9u, 6u, 0u, &lower, &upper) == VF2_OK);
        CHECK(lower == UINT32_C(0x00004330));
        CHECK(upper == UINT32_C(0x00004e20));
        CHECK(vf2_orchestrator_select_limits(0u, 9u, 8u, 255u, &lower, &upper) == VF2_OK);
        CHECK(lower == UINT32_C(0x00004330));
        CHECK(upper == UINT32_C(0x00004e20));
        CHECK(vf2_orchestrator_select_limits(0u, 9u, 0u, 7u, &lower, &upper) == VF2_OK);
        CHECK(lower == UINT32_C(0x00004330));
        CHECK(upper == 0u);
        CHECK(vf2_orchestrator_select_limits(0u, 9u, 0u, 8u, &lower, &upper) == VF2_OK);
        CHECK(lower == UINT32_C(0x00003e80));
        CHECK(upper == UINT32_C(0x00004e20));
        CHECK(vf2_orchestrator_select_limits(0u, 9u, 7u, 0u, &lower, &upper) == VF2_OK);
        CHECK(lower == UINT32_C(0x00004330));
        CHECK(upper == 0u);
        CHECK(vf2_orchestrator_select_limits(0u, 9u, 7u, 8u, &lower, &upper) == VF2_OK);
        CHECK(lower == UINT32_C(0x00003e80));
        CHECK(upper == UINT32_C(0x00004e20));
    }

    /* Invalid argument checks. */
    {
        uint32_t lower = 0u, upper = 0u;
        CHECK(vf2_orchestrator_select_limits(0u, 0u, 0u, 0u, NULL, &upper) == VF2_ERROR_INVALID_ARGUMENT);
        CHECK(vf2_orchestrator_select_limits(0u, 0u, 0u, 0u, &lower, NULL) == VF2_ERROR_INVALID_ARGUMENT);
        CHECK(vf2_orchestrator_select_default_limits(0u, 0u, NULL, &upper) == VF2_ERROR_INVALID_ARGUMENT);
        CHECK(vf2_orchestrator_select_default_limits(0u, 0u, &lower, NULL) == VF2_ERROR_INVALID_ARGUMENT);
    }
}

static void test_machine_application(void)
{
    vf2_model2a machine;
    vf2_orchestrator_limits_report report;
    uint32_t value = 0u;
    uint8_t mode = 0u;
    uint8_t field64 = 0u;
    uint8_t field31 = 0u;

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
    field64 = 0u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50064, &field64, sizeof(field64)) == VF2_OK);
    field31 = 0u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50031, &field31, sizeof(field31)) == VF2_OK);
    CHECK(
        vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK
    );
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LIMITS_ENTRY);
    CHECK(report.runtime_flags == 0u);
    CHECK(report.display_mode == 0u);
    CHECK(report.lower_limit == UINT32_C(0x00003e80));
    CHECK(report.upper_limit == UINT32_C(0x00004e20));
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

    /* Mode 6 -> 0x12a8/0x4330 */
    mode = 6u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_DISPLAY_MODE, &mode, sizeof(mode)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == UINT32_C(0x000012a8));
    CHECK(report.upper_limit == UINT32_C(0x00004330));
    CHECK(vf2_model2a_read_u32(&machine, VF2_ORCHESTRATOR_LIMIT_LOW, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x000012a8));
    CHECK(vf2_model2a_read_u32(&machine, VF2_ORCHESTRATOR_LIMIT_HIGH, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x00004330));

    /* Mode 14 -> 0x32c8/0x4e20 */
    mode = 14u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_DISPLAY_MODE, &mode, sizeof(mode)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == UINT32_C(0x000032c8));
    CHECK(report.upper_limit == UINT32_C(0x00004e20));

    /* Mode 12 -> 0/0x4e20 */
    mode = 12u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_DISPLAY_MODE, &mode, sizeof(mode)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == 0u);
    CHECK(report.upper_limit == UINT32_C(0x00004e20));

    /* Runtime bit16 overrides everything to 0/0x4e20 */
    CHECK(vf2_model2a_write_u32(&machine, VF2_ORCHESTRATOR_RUNTIME_FLAGS, UINT32_C(1) << 16u) == VF2_OK);
    mode = 6u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_DISPLAY_MODE, &mode, sizeof(mode)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == 0u);
    CHECK(report.upper_limit == UINT32_C(0x00004e20));

    /* Mode 9 with field_50064=6 -> 0x4330/0x4e20 */
    CHECK(vf2_model2a_write_u32(&machine, VF2_ORCHESTRATOR_RUNTIME_FLAGS, 0u) == VF2_OK);
    mode = 9u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_DISPLAY_MODE, &mode, sizeof(mode)) == VF2_OK);
    field64 = 6u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50064, &field64, sizeof(field64)) == VF2_OK);
    field31 = 0u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50031, &field31, sizeof(field31)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == UINT32_C(0x00004330));
    CHECK(report.upper_limit == UINT32_C(0x00004e20));

    /* Mode 9 with field_50064=0, field_50031=0 -> 0x4330/0 */
    field64 = 0u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50064, &field64, sizeof(field64)) == VF2_OK);
    field31 = 0u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50031, &field31, sizeof(field31)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == UINT32_C(0x00004330));
    CHECK(report.upper_limit == 0u);

    /* Mode 9 with field_50031 >=8 and field_50064 not 6/8 -> default */
    field31 = 8u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50031, &field31, sizeof(field31)) == VF2_OK);
    CHECK(vf2_orchestrator_apply_default_limits(&machine, &report) == VF2_OK);
    CHECK(report.lower_limit == UINT32_C(0x00003e80));
    CHECK(report.upper_limit == UINT32_C(0x00004e20));

    /* Mode 2 (skip) must remain UNSUPPORTED and not clobber limits. */
    mode = 2u;
    field31 = 0u;
    field64 = 0u;
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_DISPLAY_MODE, &mode, sizeof(mode)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50064, &field64, sizeof(field64)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, VF2_ORCHESTRATOR_FIELD_50031, &field31, sizeof(field31)) == VF2_OK);
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

static void test_orchestrator_shell_bridges(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t halfword[2] = {0u, 0u};
    uint32_t value = 0u;
    uint32_t index = 0u;
    const uint32_t stack_start =
        VF2_WORK_RAM_BASE + UINT32_C(0x4000);
    const uint32_t stack_end = stack_start + UINT32_C(112);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    halfword[0] = 0x34u;
    halfword[1] = 0x12u;
    CHECK(
        vf2_model2a_write(
            &machine, UINT32_C(0x00550168), halfword, sizeof(halfword)
        ) == VF2_OK
    );
    halfword[0] = 1u;
    halfword[1] = 0u;
    CHECK(
        vf2_model2a_write(
            &machine, UINT32_C(0x0055016a), halfword, sizeof(halfword)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00508000), 0u
        ) == VF2_OK
    );
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x0004bd24), UINT32_C(0x00001004)
        ) == VF2_OK
    );
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(
        report.kind ==
        VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL
    );
    CHECK(report.recovered_instruction_count == UINT64_C(8));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004d2c0));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(8));
    CHECK(cpu.registers[16] == UINT32_C(0x1234));

    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502c0), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502d0), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502e0), 1u
        ) == VF2_OK
    );
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00002000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x0004bf90), UINT32_C(0x00002004)
        ) == VF2_OK
    );
    report = (vf2_hybrid_bridge_report){0};
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(
        report.kind == VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL
    );
    CHECK(report.recovered_instruction_count == UINT64_C(11));
    CHECK(cpu.ip == UINT32_C(0x0004d25c));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(
        vf2_i960_cpu_return_procedure(&cpu, &machine) == VF2_OK
    );
    CHECK(cpu.ip == UINT32_C(0x0004bfdc));
    report = (vf2_hybrid_bridge_report){0};
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN);
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00002004));
    CHECK(cpu.local_frame_depth == 0u);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00003000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x0004bb94), UINT32_C(0x00003004)
        ) == VF2_OK
    );
    report = (vf2_hybrid_bridge_report){0};
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL);
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004b8d8));
    CHECK(cpu.local_frame_depth == 2u);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00004000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x0004bc58), UINT32_C(0x00004004)
        ) == VF2_OK
    );
    cpu.registers[1] = stack_end;
    for (index = 3u; index <= 30u; ++index) {
        CHECK(
            vf2_model2a_write_u32(
                &machine,
                stack_start + (index - 3u) * UINT32_C(4),
                UINT32_C(0x52000000) + index
            ) == VF2_OK
        );
    }
    report = (vf2_hybrid_bridge_report){0};
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(
        report.kind ==
        VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE
    );
    CHECK(report.recovered_instruction_count == UINT64_C(21));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00004004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[16] == UINT32_C(0x52000010));
    CHECK(cpu.registers[30] == UINT32_C(0x5200001e));
    CHECK(cpu.executed_instructions == UINT64_C(21));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c2f0), &value
        ) == VF2_OK
    );
    CHECK((value & UINT32_C(0xffff)) == 0u);

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
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bd00));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.maximum_local_frame_depth == 1u);
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
    test_select_full_sweep_512();
    test_machine_application();
    test_orchestrator_shell_bridges();
    test_orchestrator_prefix_bridges();
    test_hybrid_bridge_poststate();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-limit test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-limit tests passed\n");
    return 0;
}
