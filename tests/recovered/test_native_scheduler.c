#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/native_runtime.h"

#define TASK_COUNT_ADDRESS UINT32_C(0x00011d94)
#define RUNTIME_FLAGS UINT32_C(0x00508000)
#define CURRENT_INDEX UINT32_C(0x00500038)
#define TIMER1 UINT32_C(0x00f00004)
#define TIMER2 UINT32_C(0x00f00008)
#define TIMER_MASK UINT32_C(0x000fffff)
#define SCHEDULER_RETURN UINT32_C(0x00010dcc)
#define MAIN_AFTER_SCHEDULER UINT32_C(0x0000a014)
#define SCRATCH_BASE UINT32_C(0x0050c000)
#define SCRATCH_STRIDE UINT32_C(0x20)

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

static void write_rom_u32(uint8_t *rom, uint32_t address, uint32_t value)
{
    rom[address + 0u] = (uint8_t)(value & UINT32_C(0xff));
    rom[address + 1u] = (uint8_t)((value >> 8u) & UINT32_C(0xff));
    rom[address + 2u] = (uint8_t)((value >> 16u) & UINT32_C(0xff));
    rom[address + 3u] = (uint8_t)((value >> 24u) & UINT32_C(0xff));
}

static int initialize_machine(vf2_model2a *machine, uint8_t **rom)
{
    *rom = (uint8_t *)calloc(VF2_MAIN_ROM_SIZE, 1u);
    CHECK(*rom != NULL);
    if (*rom == NULL) {
        return 0;
    }
    write_rom_u32(*rom, TASK_COUNT_ADDRESS, UINT32_C(29));

    CHECK(vf2_model2a_initialize(machine) != 0);
    if (machine->work_ram == NULL) {
        free(*rom);
        *rom = NULL;
        return 0;
    }
    CHECK(
        vf2_model2a_attach_main_rom(
            machine,
            *rom,
            VF2_MAIN_ROM_SIZE
        ) == VF2_OK
    );
    return 1;
}

static void initialize_scheduler_cpu(
    vf2_i960_cpu *cpu,
    size_t task_index,
    uint32_t registry
)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x0000a010));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu,
            SCHEDULER_RETURN,
            MAIN_AFTER_SCHEDULER
        ) == VF2_OK
    );
    cpu->registers[10] =
        SCRATCH_BASE + (uint32_t)task_index * SCRATCH_STRIDE;
    cpu->registers[11] = (uint32_t)task_index;
    cpu->registers[29] = registry;
}

static void initialize_common_scheduler_memory(vf2_model2a *machine)
{
    CHECK(
        vf2_model2a_write_u32(
            machine,
            RUNTIME_FLAGS,
            UINT32_C(1) << 9u
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(machine, TIMER1, TIMER_MASK) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, TIMER2, TIMER_MASK) == VF2_OK);
}

static void test_stride_scanning_transition(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint8_t *rom = NULL;
    const uint32_t current_registry = UINT32_C(0x00515200);
    const uint32_t next_registry = UINT32_C(0x00515400);
    const uint32_t current_scratch =
        SCRATCH_BASE + UINT32_C(13) * SCRATCH_STRIDE;
    uint32_t value = 0u;
    size_t index = 0u;

    if (!initialize_machine(&machine, &rom)) {
        return;
    }
    initialize_common_scheduler_memory(&machine);
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_registry + UINT32_C(0x38),
            0u
        ) == VF2_OK
    );
    for (index = 0u; index < 4u; ++index) {
        const uint32_t registry =
            current_registry + (uint32_t)index * UINT32_C(0x80);
        CHECK(
            vf2_model2a_write_u32(
                &machine,
                registry + UINT32_C(8),
                UINT32_C(0x80)
            ) == VF2_OK
        );
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            next_registry,
            UINT32_C(0x80000000)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            next_registry + UINT32_C(0x0c),
            UINT32_C(0x0001d458)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, current_scratch, 1u) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_scratch + UINT32_C(4),
            2u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_scratch + UINT32_C(8),
            3u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_scratch + UINT32_C(12),
            4u
        ) == VF2_OK
    );

    initialize_scheduler_cpu(&cpu, 13u, current_registry);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(
        vf2_native_runtime_step(
            &machine,
            &cpu,
            &state,
            &report
        ) == VF2_OK
    );

    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION);
    CHECK(report.current_task_index == 13u);
    CHECK(report.next_task_index == 17u);
    CHECK(report.descriptors_scanned == 4u);
    CHECK(report.current_registry_address == current_registry);
    CHECK(report.next_registry_address == next_registry);
    CHECK(report.recovered_instruction_count == UINT64_C(77));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(cpu.ip == UINT32_C(0x0001d458));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == 0u);
    CHECK(cpu.local_frames[1].registers[10] == current_scratch + UINT32_C(0x80));
    CHECK(cpu.local_frames[1].registers[11] == UINT32_C(17));
    CHECK(cpu.registers[29] == next_registry);
    CHECK(state.scheduler_transitions == 1u);
    CHECK(state.recovered_instruction_count == UINT64_C(77));
    CHECK(
        vf2_model2a_read_u32(&machine, CURRENT_INDEX, &value) == VF2_OK
    );
    CHECK(value == UINT32_C(17));
    CHECK(
        vf2_model2a_read_u32(
            &machine,
            current_scratch + UINT32_C(8),
            &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(4));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_second_sweep_finish(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint8_t *rom = NULL;
    const uint32_t current_registry = UINT32_C(0x00516180);
    const uint32_t inactive_registry = current_registry + UINT32_C(0x100);
    const uint32_t end_registry = inactive_registry + UINT32_C(0x100);
    const uint32_t current_scratch =
        SCRATCH_BASE + UINT32_C(27) * SCRATCH_STRIDE;
    const uint32_t inactive_scratch = current_scratch + SCRATCH_STRIDE;
    uint32_t value = 0u;

    if (!initialize_machine(&machine, &rom)) {
        return;
    }
    initialize_common_scheduler_memory(&machine);
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_registry + UINT32_C(0x38),
            0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_registry + UINT32_C(8),
            UINT32_C(0x100)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, inactive_registry, 0u) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            inactive_registry + UINT32_C(8),
            UINT32_C(0x100)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_scratch + UINT32_C(8),
            UINT32_C(9)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            current_scratch + UINT32_C(0x10),
            UINT32_C(0xaaaaaaaa)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            inactive_scratch + UINT32_C(0x10),
            UINT32_C(0xbbbbbbbb)
        ) == VF2_OK
    );

    initialize_scheduler_cpu(&cpu, 27u, current_registry);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(
        vf2_native_runtime_step(
            &machine,
            &cpu,
            &state,
            &report
        ) == VF2_OK
    );

    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH);
    CHECK(report.current_task_index == 27u);
    CHECK(report.next_task_index == 29u);
    CHECK(report.descriptors_scanned == 2u);
    CHECK(report.current_registry_address == current_registry);
    CHECK(report.next_registry_address == end_registry);
    CHECK(report.recovered_instruction_count == UINT64_C(40));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == MAIN_AFTER_SCHEDULER);
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[29] == end_registry);
    CHECK(state.scheduler_finishes == 1u);
    CHECK(state.recovered_instruction_count == UINT64_C(40));
    CHECK(
        vf2_model2a_read_u32(&machine, CURRENT_INDEX, &value) == VF2_OK
    );
    CHECK(value == UINT32_C(28));
    CHECK(
        vf2_model2a_read_u32(
            &machine,
            current_scratch + UINT32_C(8),
            &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(10));
    CHECK(
        vf2_model2a_read_u32(
            &machine,
            current_scratch + UINT32_C(0x10),
            &value
        ) == VF2_OK
    );
    CHECK(value == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine,
            inactive_scratch + UINT32_C(0x10),
            &value
        ) == VF2_OK
    );
    CHECK(value == 0u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

int main(void)
{
    test_stride_scanning_transition();
    test_second_sweep_finish();

    if (failures != 0) {
        fprintf(stderr, "%d native-scheduler test(s) failed\n", failures);
        return 1;
    }
    printf("native-scheduler tests passed\n");
    return 0;
}
