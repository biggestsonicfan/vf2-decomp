#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
