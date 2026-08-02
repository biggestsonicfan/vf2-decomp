#include "vf2/native_runtime.h"

#include <string.h>

#define VF2_NATIVE_FRAME_WAIT_POLL_ENTRY UINT32_C(0x00010f90)
#define VF2_NATIVE_INTERRUPT_RETURN_ENTRY UINT32_C(0x00000d20)
#define VF2_NATIVE_SECOND_SCHEDULER_ENTRY UINT32_C(0x0000a010)
#define VF2_NATIVE_SCHEDULER_RETURN UINT32_C(0x00010dcc)
#define VF2_NATIVE_MAIN_AFTER_SCHEDULER UINT32_C(0x0000a014)
#define VF2_NATIVE_GAME_INFO_TASK_ENTRY UINT32_C(0x0001645c)
#define VF2_NATIVE_CAMERA_RECURRING_ENTRY UINT32_C(0x0001d458)
#define VF2_NATIVE_CAMERA_GATE_ENTRY UINT32_C(0x0001d660)
#define VF2_NATIVE_CAMERA_FAST_EXIT UINT32_C(0x0001e524)
#define VF2_NATIVE_USER_TASK_ENTRY UINT32_C(0x00029748)
#define VF2_NATIVE_SOUND_TASK_ENTRY UINT32_C(0x000439fc)
#define VF2_NATIVE_SOUND_CONTINUATION_ENTRY UINT32_C(0x00043abc)
#define VF2_NATIVE_KILL_OSAGE_TASK_ENTRY UINT32_C(0x000657dc)
#define VF2_NATIVE_OSAGE_TASK_ENTRY UINT32_C(0x000640f4)
#define VF2_NATIVE_TASK_COUNT_ADDRESS UINT32_C(0x00011d94)
#define VF2_NATIVE_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_NATIVE_CURRENT_INDEX UINT32_C(0x00500038)
#define VF2_NATIVE_TIMER1 UINT32_C(0x00f00004)
#define VF2_NATIVE_TIMER2 UINT32_C(0x00f00008)
#define VF2_NATIVE_TIMER_MASK UINT32_C(0x000fffff)
#define VF2_NATIVE_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_NATIVE_SCRATCH_STRIDE UINT32_C(0x20)

static void accumulate_step(
    vf2_native_runtime_state *state,
    const vf2_native_runtime_step_report *report
)
{
    ++state->blocks_executed;
    state->recovered_instruction_count += report->recovered_instruction_count;
    state->recovered_procedure_calls += report->recovered_procedure_calls;
    state->recovered_procedure_returns += report->recovered_procedure_returns;

    if (report->kind == VF2_NATIVE_RUNTIME_STEP_TASK) {
        ++state->task_bodies_executed;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
        ++state->frame_wait_phases;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER) {
        ++state->scheduler_entries;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION) {
        ++state->scheduler_transitions;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH) {
        ++state->scheduler_finishes;
    }
}

static vf2_status execute_recurring_camera_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_step_report *report
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t recurring_fighter_cursor = cpu->registers[23];
    vf2_hybrid_block_report block_report;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_CAMERA_RECURRING_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&block_report, 0, sizeof(block_report));
    status = vf2_hybrid_camera_execute(
        machine, cpu, cpu->registers[29], &block_report
    );
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_CAMERA_GATE_ENTRY) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    /* The shared update helper models the first camera invocation, where the
     * initializer leaves g7 one fighter profile behind. A recurring invocation
     * already enters with the final cursor, so preserve it across the update. */
    if (status == VF2_OK) {
        cpu->registers[23] = recurring_fighter_cursor;
        memset(&block_report, 0, sizeof(block_report));
        status = vf2_hybrid_camera_execute(
            machine, cpu, cpu->registers[29], &block_report
        );
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_CAMERA_FAST_EXIT) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
        report->task_kind = VF2_HYBRID_TASK_CAMERA;
        report->exit_address = cpu->ip;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
    }
    return status;
}

static vf2_status execute_sound_continuation_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_step_report *report
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t registry = cpu->registers[29];
    uint8_t mode = 0u;
    uint8_t global_flag = 0u;
    uint8_t zero_byte = 0u;
    uint8_t counter_bytes[2] = {0u, 0u};
    uint8_t zero_counter[2] = {0u, 0u};
    int16_t counter = 0;
    uint32_t read_pointer = 0u;
    uint32_t write_pointer = 0u;
    uint32_t value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_SOUND_CONTINUATION_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500f00), &mode, sizeof(mode)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050406b), &global_flag, sizeof(global_flag)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504070), counter_bytes,
            sizeof(counter_bytes)
        );
    }
    if (status == VF2_OK) {
        counter = (int16_t)((uint16_t)counter_bytes[0] |
            ((uint16_t)counter_bytes[1] << 8u));
        status = vf2_model2a_read_u32(
            machine, registry + UINT32_C(0x7c), &read_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, registry + UINT32_C(0x78), &write_pointer
        );
    }
    if (status == VF2_OK &&
        (mode == 1u || (global_flag & UINT8_C(1)) != 0u ||
         counter > 0 || read_pointer != write_pointer)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, read_pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504070), zero_counter,
            sizeof(zero_counter)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00504074), value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, read_pointer, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050406b), &zero_byte, sizeof(zero_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        cpu->executed_instructions += UINT64_C(20);
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
        report->task_kind = VF2_HYBRID_TASK_SOUND;
        report->exit_address = cpu->ip;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
    }
    return status;
}

static vf2_status execute_second_sweep_scheduler_finish(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_step_report *report
)
{
    const size_t current_index = (size_t)cpu->registers[11];
    const uint32_t current_registry = cpu->registers[29];
    const uint32_t current_scratch = cpu->registers[10];
    uint32_t task_count = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t scratch2 = 0u;
    uint32_t inactive_stride = 0u;
    uint32_t inactive_registry = 0u;
    uint32_t inactive_flags = 0u;
    uint32_t end_stride = 0u;
    uint32_t end_registry = 0u;
    uint32_t inactive_scratch = 0u;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_SCHEDULER_RETURN ||
        cpu->local_frame_depth != 1u ||
        current_index != 27u ||
        current_registry != UINT32_C(0x00516180) ||
        current_scratch != VF2_NATIVE_SCRATCH_BASE +
            UINT32_C(27) * VF2_NATIVE_SCRATCH_STRIDE) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_NATIVE_TASK_COUNT_ADDRESS, &task_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_NATIVE_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER1, &timer1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER2, &timer2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry + UINT32_C(0x38), &threshold
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(8), &scratch2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry + UINT32_C(8), &inactive_stride
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    inactive_registry = current_registry + inactive_stride;
    inactive_scratch = current_scratch + VF2_NATIVE_SCRATCH_STRIDE;
    status = vf2_model2a_read_u32(
        machine, inactive_registry, &inactive_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, inactive_registry + UINT32_C(8), &end_stride
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    end_registry = inactive_registry + end_stride;

    if (task_count != UINT32_C(29) ||
        (runtime_flags & (UINT32_C(1) << 9u)) == 0u ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        (timer2 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        threshold != 0u || inactive_stride == 0u || end_stride == 0u ||
        (inactive_flags & UINT32_C(0x80000000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch2;
    status = vf2_model2a_write_u32(
        machine, current_scratch + UINT32_C(8), scratch2
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, current_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_NATIVE_CURRENT_INDEX, UINT32_C(28)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_NATIVE_TIMER1, VF2_NATIVE_TIMER_MASK
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, inactive_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[29] = end_registry;
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    cpu->executed_instructions += UINT64_C(39);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_MAIN_AFTER_SCHEDULER) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH;
    report->exit_address = cpu->ip;
    report->current_task_index = current_index;
    report->next_task_index = 29u;
    report->descriptors_scanned = 2u;
    report->current_registry_address = current_registry;
    report->next_registry_address = end_registry;
    report->recovered_instruction_count = UINT64_C(40);
    report->recovered_procedure_calls = UINT64_C(0);
    report->recovered_procedure_returns = UINT64_C(1);
    return VF2_OK;
}

static vf2_status execute_second_sweep_scheduler_transition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_step_report *report
)
{
    const size_t current_index = (size_t)cpu->registers[11];
    const uint32_t current_registry = cpu->registers[29];
    const uint32_t current_scratch = cpu->registers[10];
    uint32_t task_count = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t scratch0 = 0u;
    uint32_t scratch1 = 0u;
    uint32_t scratch2 = 0u;
    uint32_t scratch3 = 0u;
    uint32_t next_registry = current_registry;
    uint32_t next_scratch = current_scratch;
    uint32_t next_entry = 0u;
    size_t next_index = current_index;
    size_t scanned = 0u;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_SCHEDULER_RETURN ||
        cpu->local_frame_depth != 1u ||
        current_scratch != VF2_NATIVE_SCRATCH_BASE +
            (uint32_t)current_index * VF2_NATIVE_SCRATCH_STRIDE) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_NATIVE_TASK_COUNT_ADDRESS, &task_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_NATIVE_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER1, &timer1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER2, &timer2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry + UINT32_C(0x38), &threshold
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, current_scratch, &scratch0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(4), &scratch1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(8), &scratch2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(12), &scratch3
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (task_count != UINT32_C(29) || current_index >= task_count ||
        (runtime_flags & (UINT32_C(1) << 9u)) == 0u ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        (timer2 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        threshold != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch2;
    status = vf2_model2a_write_u32(
        machine, current_scratch + UINT32_C(8), scratch2
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, current_scratch + UINT32_C(0x10), 0u
        );
    }

    while (status == VF2_OK && next_index + 1u < task_count) {
        uint32_t stride = 0u;
        uint32_t flags = 0u;

        status = vf2_model2a_read_u32(
            machine, next_registry + UINT32_C(8), &stride
        );
        if (status != VF2_OK || stride == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        next_registry += stride;
        next_scratch += VF2_NATIVE_SCRATCH_STRIDE;
        ++next_index;
        ++scanned;

        status = vf2_model2a_write_u32(
            machine, VF2_NATIVE_CURRENT_INDEX, (uint32_t)next_index
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, VF2_NATIVE_TIMER1, VF2_NATIVE_TIMER_MASK
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, next_registry, &flags);
        }
        if (status != VF2_OK) {
            return status;
        }

        if ((flags & UINT32_C(0x80000000)) != 0u) {
            status = vf2_model2a_read_u32(
                machine, next_registry + UINT32_C(0x0c), &next_entry
            );
            break;
        }

        status = vf2_model2a_write_u32(
            machine, next_scratch + UINT32_C(0x10), 0u
        );
    }

    if (status != VF2_OK || scanned == 0u || next_entry == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[3] = task_count;
    cpu->registers[4] = next_entry;
    cpu->registers[5] = scratch1;
    cpu->registers[6] = scratch2;
    cpu->registers[7] = scratch3;
    cpu->registers[8] = VF2_NATIVE_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = next_scratch;
    cpu->registers[11] = (uint32_t)next_index;
    cpu->registers[12] = 0u;
    cpu->registers[13] = VF2_NATIVE_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] = scanned > 1u ? timer2 : threshold;
    cpu->registers[29] = next_registry;
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    cpu->executed_instructions +=
        (uint64_t)scanned * UINT64_C(16) + UINT64_C(12);

    status = vf2_i960_cpu_enter_procedure(
        cpu, next_entry, VF2_NATIVE_SCHEDULER_RETURN
    );
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION;
    report->exit_address = cpu->ip;
    report->current_task_index = current_index;
    report->next_task_index = next_index;
    report->descriptors_scanned = scanned;
    report->current_registry_address = current_registry;
    report->next_registry_address = next_registry;
    report->recovered_instruction_count =
        (uint64_t)scanned * UINT64_C(16) + UINT64_C(13);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(0);
    (void)scratch0;
    return VF2_OK;
}

vf2_status vf2_native_runtime_initialize(
    vf2_native_runtime_state *state,
    size_t frame_wait_visits_before_interrupt
)
{
    vf2_status status = VF2_OK;

    if (state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    status = vf2_hybrid_frame_wait_initialize(
        &state->frame_wait, frame_wait_visits_before_interrupt
    );
    if (status != VF2_OK) {
        memset(state, 0, sizeof(*state));
    }
    return status;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = cpu->ip;

    if (cpu->ip == VF2_NATIVE_FRAME_WAIT_POLL_ENTRY ||
        cpu->ip == VF2_NATIVE_INTERRUPT_RETURN_ENTRY) {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = vf2_hybrid_frame_wait_execute(
            machine, cpu, &state->frame_wait, &bridge_report
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_NATIVE_SECOND_SCHEDULER_ENTRY) {
        vf2_hybrid_second_scheduler_report scheduler_report;
        memset(&scheduler_report, 0, sizeof(scheduler_report));
        status = vf2_hybrid_second_scheduler_enter(
            machine, cpu, &scheduler_report
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER;
            local_report.bridge_kind =
                VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY;
            local_report.exit_address = cpu->ip;
            local_report.next_task_index =
                scheduler_report.selected_task_index;
            local_report.next_registry_address =
                scheduler_report.selected_registry_address;
            local_report.descriptors_scanned =
                scheduler_report.descriptors_scanned;
            local_report.recovered_instruction_count =
                scheduler_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                scheduler_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                scheduler_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_NATIVE_SCHEDULER_RETURN &&
               cpu->registers[29] == UINT32_C(0x00516180)) {
        status = execute_second_sweep_scheduler_finish(
            machine, cpu, &local_report
        );
    } else if (cpu->ip == VF2_NATIVE_SCHEDULER_RETURN) {
        status = execute_second_sweep_scheduler_transition(
            machine, cpu, &local_report
        );
    } else if (cpu->ip == VF2_NATIVE_CAMERA_RECURRING_ENTRY) {
        status = execute_recurring_camera_task(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_SOUND_CONTINUATION_ENTRY) {
        status = execute_sound_continuation_task(
            machine, cpu, &local_report
        );
    } else if (cpu->ip == VF2_NATIVE_GAME_INFO_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_USER_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_SOUND_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_KILL_OSAGE_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_OSAGE_TASK_ENTRY) {
        const int recurring_kill =
            cpu->ip == VF2_NATIVE_KILL_OSAGE_TASK_ENTRY &&
            cpu->registers[29] == UINT32_C(0x00515e80);
        vf2_hybrid_task_report task_report;
        memset(&task_report, 0, sizeof(task_report));
        status = vf2_hybrid_first_dispatch_task_execute(
            machine, cpu, cpu->registers[29], &task_report
        );

        /* The recurring kill-osage path skips three first-dispatch setup
         * instructions but has identical memory and register postconditions. */
        if (status == VF2_OK && recurring_kill) {
            if (cpu->executed_instructions < UINT64_C(3) ||
                task_report.recovered_instruction_count < UINT64_C(3)) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                cpu->executed_instructions -= UINT64_C(3);
                task_report.recovered_instruction_count -= UINT64_C(3);
            }
        }
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_TASK;
            local_report.task_kind = task_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                task_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                task_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                task_report.recovered_procedure_returns;
        }
    } else {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = vf2_hybrid_post_frame_bridge_execute(
            machine, cpu, &bridge_report
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    }

    if (status == VF2_OK) {
        accumulate_step(state, &local_report);
    }
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_native_runtime_run_until(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    uint32_t stop_address,
    size_t max_blocks,
    vf2_native_runtime_run_report *report
)
{
    vf2_native_runtime_run_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = cpu->ip;
    local_report.stop_address = stop_address;
    local_report.final_address = cpu->ip;

    while (cpu->ip != stop_address && local_report.blocks_executed < max_blocks) {
        vf2_native_runtime_step_report step_report;
        memset(&step_report, 0, sizeof(step_report));
        status = vf2_native_runtime_step(
            machine, cpu, state, &step_report
        );
        if (status != VF2_OK) {
            break;
        }

        ++local_report.blocks_executed;
        local_report.recovered_instruction_count +=
            step_report.recovered_instruction_count;
        local_report.recovered_procedure_calls +=
            step_report.recovered_procedure_calls;
        local_report.recovered_procedure_returns +=
            step_report.recovered_procedure_returns;

        if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_TASK) {
            ++local_report.task_bodies_executed;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
            ++local_report.frame_wait_phases;
        } else if (
            step_report.kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER
        ) {
            ++local_report.scheduler_entries;
        } else if (
            step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION
        ) {
            ++local_report.scheduler_transitions;
        } else if (
            step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH
        ) {
            ++local_report.scheduler_finishes;
        }

        local_report.last_step_kind = step_report.kind;
        local_report.last_bridge_kind = step_report.bridge_kind;
        local_report.last_task_kind = step_report.task_kind;
        local_report.final_address = cpu->ip;
    }

    if (status == VF2_OK && cpu->ip == stop_address) {
        local_report.reached_stop = 1;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    local_report.final_address = cpu->ip;
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

const char *vf2_native_runtime_step_kind_name(
    vf2_native_runtime_step_kind kind
)
{
    switch (kind) {
    case VF2_NATIVE_RUNTIME_STEP_NONE:
        return "none";
    case VF2_NATIVE_RUNTIME_STEP_BRIDGE:
        return "bridge";
    case VF2_NATIVE_RUNTIME_STEP_TASK:
        return "task";
    case VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT:
        return "frame-wait";
    case VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER:
        return "second-scheduler";
    case VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION:
        return "scheduler-transition";
    case VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH:
        return "scheduler-finish";
    default:
        return "unknown";
    }
}
