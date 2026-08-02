#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/orchestrator_gates.h"
#include "vf2/analysis/orchestrator_scan.h"
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
            cpu, target, UINT32_C(0x00001004)
        ) == VF2_OK
    );
}

static void test_inactive_scan_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint8_t inactive[2] = {0u, 0u};
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < VF2_ORCHESTRATOR_RECORD_COUNT; ++index) {
        CHECK(
            vf2_model2a_write(
                &machine,
                VF2_ORCHESTRATOR_RECORD_START +
                    (uint32_t)index * VF2_ORCHESTRATOR_RECORD_STRIDE +
                    VF2_ORCHESTRATOR_RECORD_ACTIVE_OFFSET,
                inactive,
                sizeof(inactive)
            ) == VF2_OK
        );
    }
    enter_parent(&cpu, VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);
    CHECK(report.exit_address == VF2_ORCHESTRATOR_RECORD_SCAN_EXIT);
    CHECK(report.iterations == UINT64_C(10));
    CHECK(report.recovered_instruction_count == UINT64_C(43));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == VF2_ORCHESTRATOR_RECORD_SCAN_EXIT);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(43));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_child_gate_dispatch(
    uint32_t entry,
    uint32_t target,
    vf2_hybrid_bridge_kind expected_kind
)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};

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
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == expected_kind);
    CHECK(report.entry_address == entry);
    CHECK(report.exit_address == target);
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == target);
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(3));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_loop_gate_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};

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
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);
    CHECK(report.exit_address == VF2_ORCHESTRATOR_LOOP_GATE_EXIT);
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.recovered_instruction_count == UINT64_C(2));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == VF2_ORCHESTRATOR_LOOP_GATE_EXIT);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(2));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_header_decode_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t stream[18] = {
        0xffu, 0xffu, 0x09u, 0x04u, 0x00u, 0x82u, 0x00u, 0x00u,
        0x00u, 0x10u, 0x00u, 0x80u, 0xb4u, 0x28u, 0xc4u, 0xa3u,
        0x01u, 0x31u
    };
    uint32_t value = 0u;
    uint8_t dimensions[4] = {0u, 0u, 0u, 0u};
    const uint32_t source = UINT32_C(0x00551000);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write(
            &machine, source, stream, sizeof(stream)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004c180));
    cpu.registers[19] = source;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE);
    CHECK(report.entry_address == UINT32_C(0x0004c180));
    CHECK(report.exit_address == UINT32_C(0x0004c3f0));
    CHECK(report.iterations == UINT64_C(8));
    CHECK(report.changed_values == UINT64_C(10));
    CHECK(report.bytes_written == 36u);
    CHECK(report.recovered_instruction_count == UINT64_C(120));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004c3f0));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(120));
    CHECK(cpu.registers[3] == UINT32_C(0x0055c344));
    CHECK(cpu.registers[4] == UINT32_C(0x0055c344));
    CHECK(cpu.registers[5] == UINT32_C(9));
    CHECK(cpu.registers[7] == UINT32_C(7));
    CHECK(cpu.registers[12] == UINT32_C(0x1ff));
    CHECK(cpu.registers[13] == UINT32_C(0x00028b48));
    CHECK(cpu.registers[14] == UINT32_C(20));
    CHECK(cpu.registers[15] == source + UINT32_C(16));
    CHECK(cpu.registers[16] == UINT32_C(0x00028b40));
    CHECK(cpu.registers[18] == UINT32_C(1));
    CHECK(cpu.registers[19] == UINT32_C(0x100));
    CHECK(cpu.registers[20] == UINT32_C(0x102));
    CHECK(cpu.registers[21] == UINT32_C(0x122));
    CHECK(cpu.registers[22] == UINT32_C(0x142));
    CHECK(cpu.registers[27] == UINT32_C(0xa3c4));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(4))
    );
    CHECK(
        vf2_model2a_read(
            &machine, UINT32_C(0x0055c320), dimensions, sizeof(dimensions)
        ) == VF2_OK
    );
    CHECK(dimensions[0] == 128u && dimensions[1] == 0u);
    CHECK(dimensions[2] == 128u && dimensions[3] == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c324), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x4000));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c328), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(7));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c32c), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(4));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c330), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(9));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c334), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x82));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c338), &value
        ) == VF2_OK
    );
    CHECK(value == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c33c), &value
        ) == VF2_OK
    );
    CHECK(value == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c340), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(1));

    vf2_model2a_shutdown(&machine);
}


static void test_system_memory_diagnostic(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t low = 0u;
    uint32_t high = 0u;
    uint8_t enabled = 1u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500171), &enabled, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &mode, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c318), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c31c), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0006dcb8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC);
    CHECK(report.recovered_instruction_count == UINT64_C(75));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(report.bytes_written == 32u);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(75));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059c318), &low) == VF2_OK);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059c31c), &high) == VF2_OK);
    CHECK(low == UINT32_C(1));
    CHECK(high == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_video_input_sync(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t flags = 0u;
    uint8_t words[4] = {0x34u, 0x12u, 0x78u, 0x56u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(0x00008800)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500710), UINT32_C(8)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500700), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500714), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0100a008), words, sizeof(words)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000110f4));
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC);
    CHECK(report.recovered_instruction_count == UINT64_C(28));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.executed_instructions == UINT64_C(28));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00508000), &flags) == VF2_OK);
    CHECK(flags == UINT32_C(0x00008a00));

    vf2_model2a_shutdown(&machine);
}


static void test_frame_counter_and_phase(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t counter = 0u;
    uint32_t base = VF2_WORK_RAM_BASE + UINT32_C(0x1000);
    uint8_t shift = 9u;
    uint8_t phase = UINT8_C(0x2f);
    uint8_t next = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050d000), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050d006), &shift, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000112f8));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(21));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050d000), &counter) == VF2_OK);
    CHECK(counter == UINT32_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c), base) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, base + UINT32_C(0x3001), &phase, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00011c78));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(10));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0059c001), &next, 1u) == VF2_OK);
    CHECK(next == UINT8_C(0x30));

    vf2_model2a_shutdown(&machine);
}


static void test_frame_shadow_and_buffer_gate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t live_addresses[9] = {
        UINT32_C(0x00500234), UINT32_C(0x00500235),
        UINT32_C(0x00500236), UINT32_C(0x00500237),
        UINT32_C(0x00500238), UINT32_C(0x00500239),
        UINT32_C(0x005000e0), UINT32_C(0x005000e1),
        UINT32_C(0x005000e2)
    };
    uint32_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < UINT32_C(9); ++index) {
        uint8_t value = (uint8_t)(index + UINT32_C(1));
        CHECK(vf2_model2a_write(&machine, live_addresses[index], &value, 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x00544600) + index, &value, 1u) == VF2_OK);
    }
    enter_parent(&cpu, UINT32_C(0x00000530));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY);
    CHECK(report.recovered_instruction_count == UINT64_C(28));
    CHECK(report.iterations == UINT64_C(9));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), UINT32_C(0x00510000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), UINT32_C(0x00512000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00510000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512000), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000110b0));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(9));
    CHECK(cpu.registers[23] == UINT32_C(0x00510000));
    CHECK(cpu.registers[24] == UINT32_C(0x00512000));

    vf2_model2a_shutdown(&machine);
}


static void test_frame_dispatch_tick(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint8_t selector = 1u;
    uint32_t value = 0u;
    const size_t rom_size = (size_t)UINT32_C(0x0000a800);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0000a6fc)] = UINT8_C(0x74);
    rom[UINT32_C(0x0000a6fd)] = UINT8_C(0xa9);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &selector, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500024), UINT32_C(640)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
    CHECK(report.recovered_instruction_count == UINT64_C(14));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.executed_instructions == UINT64_C(14));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050002c), &value) == VF2_OK);
    CHECK(value == UINT32_C(2));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(639));

    vf2_model2a_shutdown(&machine);
}


static void test_game_event_queue_write(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t count = UINT8_C(2);
    uint8_t index = UINT8_C(3);
    uint32_t event = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00504001), &count, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00504003), &index, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000438ec));
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x009e0a7f);

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_EVENT_QUEUE_WRITE);
    CHECK(report.recovered_instruction_count == UINT64_C(19));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.bytes_written == 22u);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(19));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00504001), &count, 1u) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00504003), &index, 1u) == VF2_OK);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050402c), &event) == VF2_OK);
    CHECK(count == UINT8_C(3));
    CHECK(index == UINT8_C(4));
    CHECK(event == UINT32_C(0x009e0a7f));

    vf2_model2a_shutdown(&machine);
}


static void test_texture_maintenance(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t first_base = UINT32_C(0x00551000);
    const uint32_t second_base = UINT32_C(0x00552000);
    const uint8_t first_record[2] = {1u, 0u};
    const uint8_t second_record[2] = {2u, 0u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00550188), first_record, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005501a8), second_record, 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050083c), UINT32_C(11)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500840), UINT32_C(12)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), first_base) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), second_base) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, first_base + UINT32_C(0x640), UINT32_C(7)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, second_base + UINT32_C(0x640), UINT32_C(8)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004b8d8));

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_MAINTENANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(17));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(17));
    CHECK(cpu.procedure_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns == UINT64_C(3));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x005501a8));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(12));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == second_base);
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.registers[4] == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_texture_upload_and_entry_gate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint8_t zero_short[2] = {0u, 0u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502a8), zero_short, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502b0), zero_short, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502b8), zero_short, 2u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004ba80));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH);
    CHECK(report.recovered_instruction_count == UINT64_C(10));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00002de4));
    CHECK(cpu.local_frame_depth == 2u);

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055000c), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004bd00));
    report = (vf2_hybrid_bridge_report){0};
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(2));
    CHECK(cpu.ip == UINT32_C(0x0004bd24));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_texture_record_status_setup(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    enter_parent(&cpu, UINT32_C(0x0004bd5c));
    cpu.registers[3] = 0u;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP);
    CHECK(report.entry_address == UINT32_C(0x0004bd5c));
    CHECK(report.exit_address == UINT32_C(0x0004bde0));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.bytes_written == 0u);
    CHECK(cpu.ip == UINT32_C(0x0004bde0));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(1));

    vf2_model2a_shutdown(&machine);
}


static void test_texture_stream_header_call(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t pointer_cell = UINT32_C(0x00551000);
    const uint32_t stream = UINT32_C(0x00552000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, pointer_cell, stream) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, stream, 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004be6c));
    cpu.registers[10] = pointer_cell;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL);
    CHECK(report.recovered_instruction_count == UINT64_C(5));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004c180));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == stream + UINT32_C(4));

    vf2_model2a_shutdown(&machine);
}


static void test_game_threshold_evaluate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    const size_t rom_size = (size_t)UINT32_C(0x00002a00);
    const uint32_t base = VF2_WORK_RAM_BASE + UINT32_C(0x1000);
    const uint8_t mode = 0u;
    const uint8_t variant = 0u;
    const uint8_t numerator0 = UINT8_C(4);
    const uint8_t numerator1 = UINT8_C(6);
    const uint8_t offset0 = UINT8_C(1);
    const uint8_t offset1 = UINT8_C(1);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x000027ac)] = 0u;
    rom[UINT32_C(0x000027ad)] = 1u;
    rom[UINT32_C(0x000027e0)] = 0u;
    rom[UINT32_C(0x000027e1)] = 0u;
    rom[UINT32_C(0x000027e2)] = UINT8_C(0x10);
    rom[UINT32_C(0x000027e3)] = 0u;
    rom[UINT32_C(0x000027e4)] = 0u;
    rom[UINT32_C(0x000027e5)] = 0u;
    rom[UINT32_C(0x000027e6)] = UINT8_C(0x20);
    rom[UINT32_C(0x000027e7)] = 0u;
    rom[UINT32_C(0x000028b8)] = 0u;
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0050016c), base
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, base + UINT32_C(0x3320), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3350), &variant,
            sizeof(variant)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3380), &numerator0,
            sizeof(numerator0)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3388), &numerator1,
            sizeof(numerator1)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3385), &offset0,
            sizeof(offset0)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x338d), &offset1,
            sizeof(offset1)
        ) == VF2_OK
    );

    enter_parent(&cpu, UINT32_C(0x000028d4));
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE);
    CHECK(report.entry_address == UINT32_C(0x000028d4));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(2));
    CHECK(report.bytes_written == 8u);
    CHECK(report.recovered_instruction_count == UINT64_C(125));
    CHECK(report.recovered_procedure_calls == UINT64_C(5));
    CHECK(report.recovered_procedure_returns == UINT64_C(6));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(125));
    CHECK(cpu.procedure_calls == UINT64_C(6));
    CHECK(cpu.procedure_returns == UINT64_C(6));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.registers[17] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    vf2_model2a_shutdown(&machine);
    free(rom);
}


static void test_active_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint32_t active_flags = UINT32_MAX;
    const uint32_t record = UINT32_C(0x00550168);
    const uint32_t stream = UINT32_C(0x00551000);
    const uint32_t stream_word = UINT32_C(0x12340004);
    const uint8_t count[2] = {3u, 0u};
    const size_t rom_size = (size_t)UINT32_C(0x0004c200);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0004c128)] = 10u;
    rom[UINT32_C(0x0004c129)] = 0u;
    rom[UINT32_C(0x0004c12a)] = 0xfdu;
    rom[UINT32_C(0x0004c12b)] = 0xffu;
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x10), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, record + UINT32_C(2), count, sizeof(count)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x14), stream
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x18), UINT32_C(0x00552000)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, stream, stream_word) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004bde0));
    cpu.registers[5] = record;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL);
    CHECK(report.entry_address == UINT32_C(0x0004bde0));
    CHECK(report.exit_address == UINT32_C(0x0004d16c));
    CHECK(report.recovered_instruction_count == UINT64_C(22));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.bytes_written == 4u);
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004d16c));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(22));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.local_frames[1].registers[2] == UINT32_C(0x0004be6c));
    CHECK(cpu.local_frames[1].registers[5] == record);
    CHECK(cpu.local_frames[1].registers[6] == 1u);
    CHECK(cpu.local_frames[1].registers[7] == 0u);
    CHECK(cpu.local_frames[1].registers[8] == 3u);
    CHECK(cpu.local_frames[1].registers[9] == stream);
    CHECK(cpu.local_frames[1].registers[10] == UINT32_C(0x00552000));
    CHECK(cpu.registers[16] == UINT32_C(0x12));
    CHECK(cpu.registers[17] == UINT32_C(0x34));
    CHECK(cpu.registers[18] == stream_word);
    CHECK(cpu.registers[22] == UINT32_C(28));
    CHECK(cpu.registers[23] == UINT32_C(49));
    CHECK(cpu.registers[24] == 0u);
    CHECK(cpu.arithmetic_control == arithmetic_before);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c2f4), &active_flags
        ) == VF2_OK
    );
    CHECK(active_flags == 0u);

    free(rom);
    vf2_model2a_shutdown(&machine);
}


static void test_record_advance_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t pointer0 = 0u;
    uint32_t pointer1 = 0u;
    uint8_t count_bytes[2] = {3u, 0u};
    uint8_t final_count[2] = {0u, 0u};
    const uint32_t record = UINT32_C(0x00550168);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write(
            &machine, record + UINT32_C(2), count_bytes, sizeof(count_bytes)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bf60));
    cpu.registers[5] = record;
    cpu.registers[6] = 1u;
    cpu.registers[8] = 3u;
    cpu.registers[9] = UINT32_C(0x00551000);
    cpu.registers[10] = UINT32_C(0x00552000);
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE);
    CHECK(report.entry_address == UINT32_C(0x0004bf60));
    CHECK(report.exit_address == UINT32_C(0x0004bd24));
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(3));
    CHECK(report.bytes_written == 10u);
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bd24));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(12));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(3));
    CHECK(cpu.registers[6] == 0u);
    CHECK(cpu.registers[8] == 2u);
    CHECK(cpu.registers[9] == UINT32_C(0x00551004));
    CHECK(cpu.registers[10] == UINT32_C(0x00552004));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(2))
    );
    CHECK(
        vf2_model2a_read(
            &machine, record + UINT32_C(2), final_count, sizeof(final_count)
        ) == VF2_OK
    );
    CHECK(final_count[0] == 2u && final_count[1] == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, record + UINT32_C(0x14), &pointer0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, record + UINT32_C(0x18), &pointer1
        ) == VF2_OK
    );
    CHECK(pointer0 == UINT32_C(0x00551004));
    CHECK(pointer1 == UINT32_C(0x00552004));

    vf2_model2a_shutdown(&machine);
}


static void test_color_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t zero = 0u;
    uint8_t dimensions[4] = {8u, 0u, 6u, 0u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550004), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &zero, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f0000c), UINT32_C(0x000ffffe)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550008), 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550080), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f4), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0055c320), dimensions, sizeof(dimensions)) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004cd18));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE);
    CHECK(report.entry_address == UINT32_C(0x0004cd18));
    CHECK(report.exit_address == UINT32_C(0x0004cdb0));
    CHECK(report.recovered_instruction_count == UINT64_C(33));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004cdb0));
    CHECK(cpu.executed_instructions == UINT64_C(33));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[11] == UINT32_C(0x0055c2f8));
    CHECK(cpu.registers[12] == UINT32_C(8));
    CHECK(cpu.registers[13] == UINT32_C(6));
    CHECK(cpu.registers[14] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[15] == UINT32_C(0x000ffffe));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.registers[26] == UINT32_C(2048));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    vf2_model2a_shutdown(&machine);
}


static void test_word_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t zero = 0u;
    uint8_t wait = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550004), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &zero, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f0000c), UINT32_C(0x000fffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550008), 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550080), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f4), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c340), UINT32_C(7)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f8), UINT32_C(9)) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004cb64));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE);
    CHECK(report.entry_address == UINT32_C(0x0004cb64));
    CHECK(report.exit_address == UINT32_C(0x0004cc28));
    CHECK(report.recovered_instruction_count == UINT64_C(34));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.bytes_written == 5u);
    CHECK(cpu.ip == UINT32_C(0x0004cc28));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(34));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[8] == UINT32_C(7));
    CHECK(cpu.registers[9] == UINT32_C(0x005502f0));
    CHECK(cpu.registers[11] == UINT32_C(9));
    CHECK(cpu.registers[13] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[14] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[15] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050008c), &wait, 1u) == VF2_OK);
    CHECK(wait == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_tree_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint32_t value = 0u;
    const size_t rom_size = (size_t)UINT32_C(0x0004ae00);
    const uint32_t stack_start = VF2_WORK_RAM_BASE + UINT32_C(0x3000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0004ad98)] = UINT8_C(0x78);
    rom[UINT32_C(0x0004ad99)] = UINT8_C(0x56);
    rom[UINT32_C(0x0004ad9a)] = UINT8_C(0x34);
    rom[UINT32_C(0x0004ad9b)] = UINT8_C(0x12);
    rom[UINT32_C(0x0004ad9c)] = UINT8_C(0xef);
    rom[UINT32_C(0x0004ad9d)] = UINT8_C(0xcd);
    rom[UINT32_C(0x0004ad9e)] = UINT8_C(0xab);
    rom[UINT32_C(0x0004ad9f)] = UINT8_C(0x90);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);

    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c344), UINT32_C(0x0055c34c)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c348), UINT32_C(0x80000011)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c34c), UINT32_C(0x90000022)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c2f4), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c33c), UINT32_C(3)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c340), UINT32_C(4)
        ) == VF2_OK
    );
    {
        const uint8_t width[2] = {8u, 0u};
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x0055c320), width, sizeof(width)
            ) == VF2_OK
        );
    }

    enter_parent(&cpu, UINT32_C(0x0004c544));
    CHECK(cpu.registers[1] == stack_start + UINT32_C(64));
    cpu.registers[3] = UINT32_C(0x33333333);
    cpu.registers[4] = UINT32_C(0x44444444);
    cpu.registers[13] = UINT32_C(0xabcdef00);
    cpu.registers[17] = UINT32_C(0x17171717);
    cpu.registers[22] = UINT32_C(0x22222222);
    cpu.registers[27] = UINT32_C(0x27272727);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH);
    CHECK(report.entry_address == UINT32_C(0x0004c544));
    CHECK(report.exit_address == UINT32_C(0x0004c6e0));
    CHECK(report.iterations == UINT64_C(256));
    CHECK(report.bytes_written == 2160u);
    CHECK(report.recovered_instruction_count == UINT64_C(858));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004c6e0));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.registers[1] == stack_start + UINT32_C(64));
    CHECK(cpu.executed_instructions == UINT64_C(858));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[3] == UINT32_C(0x33333333));
    CHECK(cpu.registers[4] == UINT32_C(0x44444444));
    CHECK(cpu.registers[5] == UINT32_C(0x00545000));
    CHECK(cpu.registers[6] == UINT32_C(3));
    CHECK(cpu.registers[7] == 0u);
    CHECK(cpu.registers[8] == 0u);
    CHECK(cpu.registers[9] == UINT32_C(0x0055c2ee));
    CHECK(cpu.registers[10] == UINT32_C(0x0055c2ef));
    CHECK(cpu.registers[11] == UINT32_C(0x005502f0));
    CHECK(cpu.registers[12] == UINT32_C(7));
    CHECK(cpu.registers[13] == UINT32_C(0xabcdef00));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.registers[17] == UINT32_C(0x17171717));
    CHECK(cpu.registers[20] == UINT32_C(0x81000011));
    CHECK(cpu.registers[21] == UINT32_C(0x12345678));
    CHECK(cpu.registers[22] == UINT32_C(0x22222222));
    CHECK(cpu.registers[26] == UINT32_C(4));
    CHECK(cpu.registers[27] == UINT32_C(0x27272727));
    CHECK(cpu.registers[30] == UINT32_C(8));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00545000), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x81000011));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00545004), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x12345678));

    free(rom);
    vf2_model2a_shutdown(&machine);
}


static void test_counter_update_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t counter0 = UINT32_MAX;
    uint32_t counter1 = UINT32_MAX;
    uint32_t counter2 = UINT32_MAX;
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
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
            &machine, UINT32_C(0x005502e0), UINT32_C(3)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bb98));
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x0004bb98));
    CHECK(report.exit_address == UINT32_C(0x0004bc58));
    CHECK(report.iterations == UINT64_C(3));
    CHECK(report.changed_values == UINT64_C(1));
    CHECK(report.bytes_written == 4u);
    CHECK(report.recovered_instruction_count == UINT64_C(14));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bc58));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(14));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[3] == UINT32_C(0x005502e0));
    CHECK(cpu.registers[4] == UINT32_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(4))
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502c0), &counter0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502d0), &counter1
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502e0), &counter2
        ) == VF2_OK
    );
    CHECK(counter0 == 0u);
    CHECK(counter1 == 0u);
    CHECK(counter2 == UINT32_C(2));

    vf2_model2a_shutdown(&machine);
}


int main(void)
{
    test_system_memory_diagnostic();
    test_video_input_sync();
    test_frame_counter_and_phase();
    test_frame_shadow_and_buffer_gate();
    test_frame_dispatch_tick();
    test_game_event_queue_write();
    test_texture_maintenance();
    test_texture_upload_and_entry_gate();
    test_texture_record_status_setup();
    test_texture_stream_header_call();
    test_game_threshold_evaluate();
    test_inactive_scan_dispatch();
    test_child_gate_dispatch(
        VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY,
        VF2_ORCHESTRATOR_CHILD_GATE_A_TARGET,
        VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A
    );
    test_child_gate_dispatch(
        VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY,
        VF2_ORCHESTRATOR_CHILD_GATE_B_TARGET,
        VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B
    );
    test_loop_gate_dispatch();
    test_header_decode_dispatch();
    test_color_prepare_dispatch();
    test_word_prepare_dispatch();
    test_tree_dispatch();
    test_active_prepare_dispatch();
    test_record_advance_dispatch();
    test_counter_update_dispatch();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-bridge test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-bridge tests passed\n");
    return 0;
}
