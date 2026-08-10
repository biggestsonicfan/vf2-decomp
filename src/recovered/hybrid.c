#include "vf2/hybrid.h"

#include <string.h>

#include "vf2/recovered.h"

#define VF2_CAMERA_INITIALIZE_ENTRY UINT32_C(0x0001d320)
#define VF2_CAMERA_INITIALIZE_EXIT UINT32_C(0x0001d458)
#define VF2_CAMERA_UPDATE_ENTRY UINT32_C(0x0001d458)
#define VF2_CAMERA_UPDATE_EXIT UINT32_C(0x0001d660)
#define VF2_CAMERA_GATE_ENTRY UINT32_C(0x0001d660)
#define VF2_CAMERA_GATE_FAST_EXIT UINT32_C(0x0001e524)
#define VF2_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_PLAYER_TASK_ENTRY UINT32_C(0x00013f08)
#define VF2_TASK_CAMERA_ENTRY UINT32_C(0x0001d320)
#define VF2_TASK_USER_ENTRY UINT32_C(0x00029748)
#define VF2_TASK_SOUND_ENTRY UINT32_C(0x000439fc)
#define VF2_TASK_KILL_OSAGE_ENTRY UINT32_C(0x000657dc)
#define VF2_TASK_OSAGE_ENTRY UINT32_C(0x000640f4)
#define VF2_PLAYER_TASK_WRAPPER_ENTRY UINT32_C(0x000142f4)
#define VF2_SCHEDULER_RETURN UINT32_C(0x00010dcc)
#define VF2_INTERPRETED_TASK_STEP_LIMIT UINT64_C(20000000)

static vf2_status hybrid_game_info_interpreter_needed(
    const vf2_model2a *machine,
    int *needed
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || needed == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        *needed = ((fighter0_flags | fighter1_flags) &
                   UINT32_C(0x80000000)) != 0u;
    }
    return status;
}

/* Keep unrecovered fighter tasks exact while their C recovery is pending:
 * execute the original task until its architectural RET returns to the
 * scheduler. This is an explicit bridge, not a silent native fallback. */
static vf2_status hybrid_execute_interpreted_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    uint32_t entry_address,
    vf2_recovered_task_report *report
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != entry_address || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = VF2_SCHEDULER_RETURN;
    options.max_steps = VF2_INTERPRETED_TASK_STEP_LIMIT;
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    if (result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu->ip != VF2_SCHEDULER_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(report, 0, sizeof(*report));
    report->entry_point = entry_address;
    report->registry_address = registry_address;
    report->continuation = cpu->ip;
    (void)start_instructions;
    (void)start_calls;
    (void)start_returns;
    return VF2_OK;
}

/* The bit-31 fa_game_info path is a dispatcher around the two large fighter
 * procedures at 0x18144 and 0x18644.  Keep those procedures ROM-backed for
 * now, but recover the dispatcher and its small post-call tail in C.  Each
 * child is entered with the same architectural call frame the ROM CALL would
 * create and is run only to its observed return address. */
static vf2_status hybrid_execute_game_info_child(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t target,
    uint32_t return_address
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != return_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);
    if (status != VF2_OK) {
        return status;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = return_address;
    options.max_steps = VF2_INTERPRETED_TASK_STEP_LIMIT;
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    if (result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu->ip != return_address) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* vf2_i960_cpu_enter_procedure accounts the architectural frame/call;
     * account the CALL instruction itself here because the caller is native. */
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_game_info_bit31_native(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_task_report *report
)
{
    const uint32_t entry_address = UINT32_C(0x0001645c);
    const uint32_t first_child = UINT32_C(0x00018144);
    const uint32_t second_child = UINT32_C(0x00018644);
    const uint32_t first_return = UINT32_C(0x0001647c);
    const uint32_t second_return = UINT32_C(0x00016494);
    const uint32_t third_return = UINT32_C(0x000164b0);
    const uint32_t task_return = UINT32_C(0x00010dcc);
    const uint32_t fighter0_slot = UINT32_C(0x00500804);
    const uint32_t fighter1_slot = UINT32_C(0x00500808);
    const uint32_t runtime_flags_address = UINT32_C(0x00508000);
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t combined_flags = 0u;
    uint8_t countdown = 0u;
    uint8_t zero = 0u;
    uint64_t native_instructions = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != entry_address || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, fighter0_slot, &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1_slot, &fighter1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, runtime_flags_address, &runtime_flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    /* 0x1645c..0x16470: four loads; the register values are observable at
     * each child boundary, so preserve the ROM aliases explicitly. */
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    native_instructions += UINT64_C(4);
    cpu->registers[7] = fighter0_flags;
    cpu->registers[8] = fighter1_flags;

    if ((fighter0_flags & UINT32_C(0x80000000)) != 0u) {
        cpu->ip = first_return;
        status = hybrid_execute_game_info_child(
            machine, cpu, first_child, first_return
        );
        native_instructions += UINT64_C(1); /* bbc */
        if (status != VF2_OK) {
            return status;
        }
    } else {
        native_instructions += UINT64_C(1); /* bbc */
    }

    /* The second pointer pair is loaded in the opposite order by the ROM. */
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter1;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter0;
    native_instructions += UINT64_C(2);
    native_instructions += UINT64_C(1); /* second bit-31 test */
    if ((fighter1_flags & UINT32_C(0x80000000)) != 0u) {
        cpu->ip = second_return;
        status = hybrid_execute_game_info_child(
            machine, cpu, first_child, second_return
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    combined_flags = fighter0_flags & fighter1_flags;
    cpu->registers[3] = combined_flags;
    native_instructions += UINT64_C(2); /* and + bbc */
    if ((combined_flags & UINT32_C(0x80000000)) != 0u) {
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
        native_instructions += UINT64_C(2);
        cpu->ip = third_return;
        status = hybrid_execute_game_info_child(
            machine, cpu, second_child, third_return
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter1;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter0;
        native_instructions += UINT64_C(2);
        cpu->ip = UINT32_C(0x000164c4);
        status = hybrid_execute_game_info_child(
            machine, cpu, second_child, UINT32_C(0x000164c4)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[15] = runtime_flags;
    native_instructions += UINT64_C(2); /* runtime load + bit-5 branch */
    if ((runtime_flags & (UINT32_C(1) << 5u)) == 0u) {
        status = vf2_model2a_write(
            machine, fighter1 + UINT32_C(0x1200), &zero, sizeof(zero)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, fighter0 + UINT32_C(0x1200), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
            );
        }
        if (status == VF2_OK) {
            native_instructions += UINT64_C(6); /* mov/stib, mov/stib, ldob/cmpobe */
            if (countdown != 0u) {
                --countdown;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
                );
                native_instructions += UINT64_C(2); /* subo/stob */
            }
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    if (cpu->ip != UINT32_C(0x000164c4) &&
        cpu->ip != UINT32_C(0x000164c8) &&
        cpu->ip != UINT32_C(0x000164cc)) {
        /* The child helper returns to 0x164c4 only on the shared-fighter
         * branch; otherwise the dispatcher is already at the runtime load. */
        cpu->ip = UINT32_C(0x000164c4);
    }
    native_instructions += UINT64_C(1); /* task RET */
    cpu->executed_instructions += native_instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != task_return) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(report, 0, sizeof(*report));
    report->entry_point = entry_address;
    report->continuation = task_return;
    return VF2_OK;
}

static vf2_status hybrid_camera_fast_gate_supported(
    const vf2_model2a *machine
)
{
    uint8_t input_index = 0u;
    uint8_t control_flags = 0u;
    uint16_t input_flags = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &input_index, sizeof(input_index)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x0006eea0) + ((uint32_t)input_index << 8u),
            &input_flags, sizeof(input_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050009c),
            &control_flags, sizeof(control_flags)
        );
    }
    if (status == VF2_OK &&
        (((input_flags & (UINT16_C(1) << 3u)) != 0u) ||
         ((control_flags & UINT8_C(1)) == 0u))) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    return status;
}

static vf2_status hybrid_camera_apply_memory(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t instruction_pointer,
    vf2_hybrid_block_report *report
)
{
    vf2_hybrid_block_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = instruction_pointer;
    local_report.registry_address = registry_address;

    switch (instruction_pointer) {
    case VF2_CAMERA_INITIALIZE_ENTRY: {
        vf2_recovered_camera_init_report recovered;
        memset(&recovered, 0, sizeof(recovered));
        status = vf2_recovered_task_camera_initialize(
            machine, registry_address, &recovered
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_HYBRID_BLOCK_CAMERA_INITIALIZE;
            local_report.exit_address = recovered.continuation;
            local_report.task_bytes_written = recovered.task_bytes_written;
            local_report.global_bytes_written = recovered.global_bytes_written;
            local_report.recovered_instruction_count = UINT64_C(2586);
            local_report.recovered_procedure_calls = UINT64_C(2);
            local_report.recovered_procedure_returns = UINT64_C(2);
        }
        break;
    }
    case VF2_CAMERA_UPDATE_ENTRY: {
        vf2_recovered_camera_update_report recovered;
        memset(&recovered, 0, sizeof(recovered));
        status = vf2_recovered_task_camera_first_update(
            machine, registry_address, &recovered
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_HYBRID_BLOCK_CAMERA_UPDATE;
            local_report.exit_address = recovered.stop_address;
            local_report.task_bytes_written = recovered.task_bytes_written;
            local_report.global_bytes_written = recovered.global_bytes_written;
            local_report.recovered_instruction_count = UINT64_C(107);
            local_report.recovered_procedure_calls = UINT64_C(4);
            local_report.recovered_procedure_returns = UINT64_C(4);
        }
        break;
    }
    case VF2_CAMERA_GATE_ENTRY: {
        vf2_recovered_camera_gate_report recovered;
        memset(&recovered, 0, sizeof(recovered));
        status = vf2_recovered_task_camera_post_update_gate(
            machine, registry_address, &recovered
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE;
            local_report.exit_address = recovered.stop_address;
            local_report.task_bytes_written = recovered.task_bytes_written;
            local_report.viewport_entries_written =
                recovered.viewport_entries_written;
            local_report.viewport_executed = recovered.viewport_executed;
            local_report.fast_exit = recovered.fast_exit;
            if (recovered.fast_exit != 0 &&
                recovered.viewport_executed == 0 &&
                recovered.stop_address == VF2_CAMERA_GATE_FAST_EXIT) {
                local_report.recovered_instruction_count = UINT64_C(6);
            }
        }
        break;
    }
    default:
        status = VF2_ERROR_UNSUPPORTED;
        break;
    }

    if (status == VF2_OK && report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_hybrid_camera_apply(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t instruction_pointer,
    vf2_hybrid_block_report *report
)
{
    return hybrid_camera_apply_memory(
        machine, registry_address, instruction_pointer, report
    );
}

static vf2_status hybrid_camera_apply_cpu_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_hybrid_block_report *report
)
{
    uint32_t value = 0u;
    uint8_t input_index = 0u;
    uint8_t control_flags = 0u;
    uint8_t range_flags = 0u;
    uint16_t input_flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    switch (report->kind) {
    case VF2_HYBRID_BLOCK_CAMERA_INITIALIZE:
        if (report->entry_address != VF2_CAMERA_INITIALIZE_ENTRY ||
            report->exit_address != VF2_CAMERA_INITIALIZE_EXIT) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cpu->registers[2] = UINT32_C(0x0001d44c);
        cpu->registers[15] = VF2_CAMERA_INITIALIZE_EXIT;
        /* g7 is the live fighter-profile cursor. The initializer leaves it on
         * fighter 0, one 0x2000-byte profile block below the entry value. */
        cpu->registers[23] -= UINT32_C(0x00002000);
        break;

    case VF2_HYBRID_BLOCK_CAMERA_UPDATE:
        if (report->entry_address != VF2_CAMERA_UPDATE_ENTRY ||
            report->exit_address != VF2_CAMERA_UPDATE_EXIT) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cpu->registers[2] = UINT32_C(0x0001d654);
        cpu->registers[13] = UINT32_C(0x3d4ccccd);
        cpu->registers[14] = UINT32_C(0x3d4ccccd);
        status = vf2_model2a_read_u32(machine, report->registry_address, &value);
        if (status == VF2_OK) {
            cpu->registers[15] = value;
            status = vf2_model2a_read_u32(
                machine, report->registry_address + UINT32_C(0x1c), &value
            );
        }
        if (status == VF2_OK) {
            cpu->registers[17] = value;
            status = vf2_model2a_read_u32(
                machine, report->registry_address + UINT32_C(0x20), &value
            );
        }
        if (status == VF2_OK) {
            cpu->registers[18] = value;
            status = vf2_model2a_read(
                machine, report->registry_address + UINT32_C(0xfa),
                &range_flags, sizeof(range_flags)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[16] = range_flags;
        /* The recurring update finishes after selecting fighter 1, restoring
         * the cursor to the adjacent profile block. */
        cpu->registers[23] += UINT32_C(0x00002000);
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        break;

    case VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE:
        if (report->entry_address != VF2_CAMERA_GATE_ENTRY ||
            report->exit_address != VF2_CAMERA_GATE_FAST_EXIT ||
            report->fast_exit == 0 || report->viewport_executed != 0) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050009c),
            &control_flags, sizeof(control_flags)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00500064),
                &input_index, sizeof(input_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine,
                UINT32_C(0x0006eea0) + ((uint32_t)input_index << 8u),
                &input_flags, sizeof(input_flags)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = control_flags;
        cpu->registers[15] = input_flags;
        break;

    case VF2_HYBRID_BLOCK_NONE:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    if (report->recovered_procedure_calls != 0u &&
        cpu->maximum_local_frame_depth < cpu->local_frame_depth + 1u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 1u;
    }
    cpu->ip = report->exit_address;
    cpu->executed_instructions += report->recovered_instruction_count;
    cpu->procedure_calls += report->recovered_procedure_calls;
    cpu->procedure_returns += report->recovered_procedure_returns;
    return VF2_OK;
}

vf2_status vf2_hybrid_camera_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_block_report *report
)
{
    vf2_hybrid_block_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->registers[29] != registry_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    if (cpu->ip == VF2_CAMERA_GATE_ENTRY) {
        status = hybrid_camera_fast_gate_supported(machine);
    }
    if (status == VF2_OK) {
        status = hybrid_camera_apply_memory(
            machine, registry_address, cpu->ip, &local_report
        );
    }
    if (status == VF2_OK) {
        status = hybrid_camera_apply_cpu_poststate(machine, cpu, &local_report);
    }
    if (status == VF2_OK) {
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}


#define VF2_TASK_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_TASK_SCHEDULER_RETURN UINT32_C(0x00010dcc)

#define VF2_SCHEDULER_TASK_COUNT_ADDRESS UINT32_C(0x00011d94)
#define VF2_SCHEDULER_TASK_NAME_TABLE UINT32_C(0x00011dd8)
#define VF2_SCHEDULER_TASK_NAME_STRIDE UINT32_C(0x40)
#define VF2_SCHEDULER_TASK_NAME_SIZE 12u
#define VF2_SCHEDULER_TASK_NAME_TEXT_SIZE 11u
#define VF2_SCHEDULER_CURRENT_INDEX UINT32_C(0x00500038)
#define VF2_SCHEDULER_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_SCHEDULER_INPUT_POINTER UINT32_C(0x00500814)
#define VF2_SCHEDULER_NAME_BUFFER UINT32_C(0x0050e000)
#define VF2_SCHEDULER_NAME_CURSOR UINT32_C(0x0050e003)
#define VF2_SCHEDULER_NAME_FORMAT UINT32_C(0x0100045c)
#define VF2_SCHEDULER_TILE_NAME_CHARS 8u
#define VF2_SCHEDULER_TILE_NAME_BYTES 18u
#define VF2_SCHEDULER_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_SCHEDULER_SCRATCH_STRIDE UINT32_C(0x20)
#define VF2_SCHEDULER_TIMER_MASK UINT32_C(0x000fffff)

vf2_status vf2_hybrid_first_dispatch_scheduler_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t current_task_index,
    size_t next_task_index,
    uint32_t current_registry_address,
    uint32_t next_registry_address,
    uint32_t next_entry_address,
    vf2_hybrid_scheduler_transition_report *report
)
{
    vf2_hybrid_scheduler_transition_report local_report;
    uint8_t task_name[VF2_SCHEDULER_TASK_NAME_SIZE];
    uint8_t tile_name[VF2_SCHEDULER_TILE_NAME_BYTES];
    uint8_t input_flags = 0u;
    uint32_t input_pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t scratch0 = 0u;
    uint32_t scratch1 = 0u;
    uint32_t scratch2 = 0u;
    uint32_t scratch3 = 0u;
    uint32_t current_scratch = 0u;
    uint32_t next_scratch = 0u;
    uint64_t scanned = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        current_task_index >= next_task_index || next_task_index >= 32u ||
        cpu->ip != VF2_TASK_SCHEDULER_RETURN ||
        cpu->local_frame_depth == 0u ||
        cpu->registers[29] != current_registry_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(task_name, 0, sizeof(task_name));
    memset(tile_name, 0, sizeof(tile_name));
    current_scratch = VF2_SCHEDULER_SCRATCH_BASE +
        (uint32_t)current_task_index * VF2_SCHEDULER_SCRATCH_STRIDE;
    next_scratch = VF2_SCHEDULER_SCRATCH_BASE +
        (uint32_t)next_task_index * VF2_SCHEDULER_SCRATCH_STRIDE;
    scanned = (uint64_t)(next_task_index - current_task_index);

    status = vf2_model2a_read_u32(
        machine, VF2_SCHEDULER_TASK_COUNT_ADDRESS, &task_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_INPUT_POINTER, &input_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, input_pointer + UINT32_C(0xde),
            &input_flags, sizeof(input_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), &timer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(8), &timer2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry_address + UINT32_C(0x38), &threshold
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
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_SCHEDULER_TASK_NAME_TABLE +
                (uint32_t)next_task_index * VF2_SCHEDULER_TASK_NAME_STRIDE,
            task_name, VF2_SCHEDULER_TASK_NAME_TEXT_SIZE
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* This bounded recovery accepts the naturally observed first-dispatch
     * path: 29 descriptors, timing enabled, diagnostic helper enabled, and
     * both timer samples still at the reload value (zero elapsed ticks). */
    if (task_count != 29u || next_task_index >= task_count ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (runtime_flags & (UINT32_C(1) << 9u)) != 0u ||
        (input_flags & (UINT8_C(1) << 2u)) != 0u ||
        (timer1 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        (timer2 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
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
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SCHEDULER_CURRENT_INDEX,
            (uint32_t)next_task_index
        );
    }
    task_name[VF2_SCHEDULER_TASK_NAME_TEXT_SIZE] = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_BUFFER,
            task_name, sizeof(task_name)
        );
    }
    if (status == VF2_OK) {
        size_t character_index = 0u;

        /* The diagnostic formatter drops the "fa_" prefix, truncates the
         * visible name to eight characters, pads it with spaces, and stores
         * each glyph as a little-endian 0x80xx tile word. The ninth word is
         * the unstyled trailing space already present in the status field. */
        for (character_index = 0u;
             character_index < VF2_SCHEDULER_TILE_NAME_CHARS;
             ++character_index) {
            const uint8_t source =
                task_name[3u + character_index] != 0u
                    ? task_name[3u + character_index]
                    : UINT8_C(0x20);
            tile_name[character_index * 2u] = source;
            tile_name[character_index * 2u + 1u] = UINT8_C(0x80);
        }
        tile_name[16] = UINT8_C(0x20);
        tile_name[17] = UINT8_C(0x00);
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_FORMAT,
            tile_name, sizeof(tile_name)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4),
            VF2_SCHEDULER_TIMER_MASK
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    memset(&cpu->registers[2], 0, 14u * sizeof(cpu->registers[0]));
    cpu->registers[3] = task_count;
    cpu->registers[4] = next_entry_address;
    cpu->registers[5] = scratch1;
    cpu->registers[6] = scratch2;
    cpu->registers[7] = scratch3;
    cpu->registers[8] = VF2_SCHEDULER_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = next_scratch;
    cpu->registers[11] = (uint32_t)next_task_index;
    cpu->registers[13] = VF2_SCHEDULER_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] =
        scanned > UINT64_C(1) ? timer2 : threshold;
    cpu->registers[16] = VF2_SCHEDULER_NAME_CURSOR;
    cpu->registers[25] = VF2_SCHEDULER_NAME_FORMAT;
    cpu->registers[29] = next_registry_address;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;

    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
    }
    cpu->executed_instructions += scanned * UINT64_C(104) + UINT64_C(12);
    cpu->procedure_calls += scanned * UINT64_C(2);
    cpu->procedure_returns += scanned * UINT64_C(2);
    status = vf2_i960_cpu_enter_procedure(
        cpu, next_entry_address, VF2_TASK_SCHEDULER_RETURN
    );
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.current_task_index = current_task_index;
    local_report.next_task_index = next_task_index;
    local_report.descriptors_scanned = (size_t)scanned;
    local_report.current_registry_address = current_registry_address;
    local_report.next_registry_address = next_registry_address;
    local_report.next_entry_address = next_entry_address;
    local_report.current_scratch_address = current_scratch;
    local_report.next_scratch_address = next_scratch;
    local_report.recovered_instruction_count =
        scanned * UINT64_C(104) + UINT64_C(13);
    local_report.recovered_procedure_calls = scanned * UINT64_C(2) + UINT64_C(1);
    local_report.recovered_procedure_returns = scanned * UINT64_C(2);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    (void)scratch0;
    return VF2_OK;
}


#define VF2_SECOND_SCHEDULER_CALL_SITE UINT32_C(0x0000a010)
#define VF2_SECOND_SCHEDULER_ENTRY UINT32_C(0x00010d54)
#define VF2_SECOND_SCHEDULER_TASK_RETURN UINT32_C(0x00010dcc)
#define VF2_SECOND_SCHEDULER_REGISTRY_BASE UINT32_C(0x00510000)
#define VF2_SECOND_SCHEDULER_GEOMETRY_STATUS UINT32_C(0x00800070)
#define VF2_SECOND_SCHEDULER_GEOMETRY_COMMAND UINT32_C(0x00804000)

static int hybrid_second_scheduler_task_supported(uint32_t entry_address)
{
    switch (entry_address) {
    case VF2_TASK_GAME_INFO_ENTRY:
    case VF2_PLAYER_TASK_ENTRY:
    case VF2_PLAYER_TASK_WRAPPER_ENTRY:
    case VF2_TASK_CAMERA_ENTRY:
    case VF2_TASK_USER_ENTRY:
    case VF2_TASK_SOUND_ENTRY:
    case VF2_TASK_KILL_OSAGE_ENTRY:
    case VF2_TASK_OSAGE_ENTRY:
        return 1;
    default:
        return 0;
    }
}

vf2_status vf2_hybrid_second_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
)
{
    vf2_hybrid_second_scheduler_report local_report;
    uint32_t ready_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t registry = VF2_SECOND_SCHEDULER_REGISTRY_BASE;
    uint32_t scratch = VF2_SCHEDULER_SCRATCH_BASE;
    uint32_t selected_entry = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        (cpu->ip != VF2_SECOND_SCHEDULER_CALL_SITE &&
         cpu->ip != VF2_SECOND_SCHEDULER_ENTRY) ||
        (cpu->ip == VF2_SECOND_SCHEDULER_CALL_SITE &&
         cpu->local_frame_depth > 1u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &ready_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_TASK_COUNT_ADDRESS, &task_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), &timer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(8), &timer2
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((ready_flags & (UINT32_C(1) << 16u)) != 0u ||
        task_count != 29u ||
        (runtime_flags & (UINT32_C(1) << 9u)) == 0u ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        (timer2 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (cpu->ip == VF2_SECOND_SCHEDULER_CALL_SITE) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_SECOND_SCHEDULER_ENTRY,
            VF2_SECOND_SCHEDULER_CALL_SITE + UINT32_C(4)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    /* The two 0x7b18 helpers clear the geometry status word and publish
     * command values 3 then 1. Only the final command remains visible, but
     * both calls and both returns are reflected in the architectural counts. */
    status = vf2_model2a_write_u32(
        machine, VF2_SECOND_SCHEDULER_GEOMETRY_STATUS, 0u
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SECOND_SCHEDULER_GEOMETRY_COMMAND, 3u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SECOND_SCHEDULER_GEOMETRY_STATUS, 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SECOND_SCHEDULER_GEOMETRY_COMMAND, 1u
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < task_count; ++index) {
        uint32_t flags = 0u;
        uint32_t stack_size = 0u;
        uint32_t elapsed = 0u;

        status = vf2_model2a_write_u32(
            machine, VF2_SCHEDULER_CURRENT_INDEX, (uint32_t)index
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, VF2_TIMER_BASE + UINT32_C(4),
                VF2_SCHEDULER_TIMER_MASK
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, registry, &flags);
        }
        if (status != VF2_OK) {
            return status;
        }
        if ((flags & UINT32_C(0x80000000)) != 0u) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(0x0c), &selected_entry
            );
            if (status != VF2_OK) {
                return status;
            }
            break;
        }

        elapsed = VF2_SCHEDULER_TIMER_MASK -
            (timer2 & VF2_SCHEDULER_TIMER_MASK);
        status = vf2_model2a_write_u32(
            machine, scratch + UINT32_C(0x10), elapsed
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(8), &stack_size
            );
        }
        if (status != VF2_OK || stack_size == 0u ||
            (stack_size & UINT32_C(0x1f)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        registry += stack_size;
        scratch += VF2_SCHEDULER_SCRATCH_STRIDE;
    }

    if (index >= task_count) {
        /* The ROM returns normally when the full registry scan finds no
         * runnable descriptor. The next frame will re-enter the scheduler;
         * there is no task frame to reconstruct in this case. */
        cpu->registers[0] &= ~UINT32_C(7);
        cpu->executed_instructions += UINT64_C(220);
        cpu->procedure_calls += UINT64_C(2);
        cpu->procedure_returns += UINT64_C(2);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK) {
            return status;
        }
        ++cpu->executed_instructions;
        local_report.descriptors_scanned = task_count;
        local_report.inactive_descriptors_scanned = task_count;
        local_report.selected_task_index = task_count;
        local_report.registry_start = VF2_SECOND_SCHEDULER_REGISTRY_BASE;
        local_report.selected_registry_address = registry;
        local_report.selected_entry_address = 0u;
        local_report.scheduler_entry_address = VF2_SECOND_SCHEDULER_ENTRY;
        local_report.recovered_instruction_count = UINT64_C(221);
        local_report.recovered_procedure_calls = UINT64_C(2);
        local_report.recovered_procedure_returns = UINT64_C(3);
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
        return VF2_OK;
    }
    if (!hybrid_second_scheduler_task_supported(selected_entry)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Reconstruct the scheduler frame at the observed callx boundary. The
     * task entry itself receives a fresh local frame, while this state remains
     * cached for the task's later RET to 0x10dcc. */
    memset(&cpu->registers[2], 0, 14u * sizeof(cpu->registers[0]));
    cpu->registers[0] = UINT32_C(0x005ff500);
    cpu->registers[2] = UINT32_C(0x00010d64);
    cpu->registers[3] = task_count;
    cpu->registers[4] = selected_entry;
    cpu->registers[8] = VF2_SCHEDULER_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = scratch;
    cpu->registers[11] = (uint32_t)index;
    cpu->registers[13] = VF2_SCHEDULER_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] = timer2;
    cpu->registers[16] = 1u;
    cpu->registers[29] = registry;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;

    cpu->procedure_calls += UINT64_C(2);
    cpu->procedure_returns += UINT64_C(2);
    if (cpu->maximum_local_frame_depth < 2u) {
        cpu->maximum_local_frame_depth = 2u;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, selected_entry, VF2_SECOND_SCHEDULER_TASK_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(235);

    local_report.descriptors_scanned = index + 1u;
    local_report.inactive_descriptors_scanned = index;
    local_report.selected_task_index = index;
    local_report.registry_start = VF2_SECOND_SCHEDULER_REGISTRY_BASE;
    local_report.selected_registry_address = registry;
    local_report.selected_entry_address = selected_entry;
    local_report.scheduler_entry_address = VF2_SECOND_SCHEDULER_ENTRY;
    local_report.recovered_instruction_count = UINT64_C(235);
    local_report.recovered_procedure_calls = UINT64_C(4);
    local_report.recovered_procedure_returns = UINT64_C(2);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

static vf2_status hybrid_complete_procedure(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t body_instructions,
    uint64_t nested_calls,
    uint64_t nested_returns
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (nested_calls != 0u &&
        cpu->maximum_local_frame_depth < cpu->local_frame_depth + 1u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 1u;
    }
    cpu->executed_instructions += body_instructions;
    cpu->procedure_calls += nested_calls;
    cpu->procedure_returns += nested_returns;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    return status;
}

static vf2_status hybrid_execute_camera_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    vf2_hybrid_task_report local_report;
    vf2_hybrid_block_report block;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t block_index = 0u;
    vf2_status status = VF2_OK;

    memset(&local_report, 0, sizeof(local_report));
    local_report.kind = VF2_HYBRID_TASK_CAMERA;
    local_report.entry_address = cpu->ip;
    local_report.registry_address = registry_address;
    start_instructions = cpu->executed_instructions;
    start_calls = cpu->procedure_calls;
    start_returns = cpu->procedure_returns;

    for (block_index = 0u; status == VF2_OK && block_index < 3u; ++block_index) {
        memset(&block, 0, sizeof(block));
        status = vf2_hybrid_camera_execute(
            machine, cpu, registry_address, &block
        );
        if (status == VF2_OK) {
            local_report.task_bytes_written += block.task_bytes_written;
            local_report.global_bytes_written += block.global_bytes_written;
            ++local_report.camera_blocks_executed;
        }
    }
    if (status == VF2_OK && cpu->ip != VF2_CAMERA_GATE_FAST_EXIT) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_complete_procedure(machine, cpu, 0u, 0u, 0u);
    }
    if (status == VF2_OK && cpu->ip != VF2_TASK_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        local_report.exit_address = cpu->ip;
        local_report.recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        local_report.recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        local_report.recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}

vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    vf2_hybrid_task_report local_report;
    vf2_recovered_task_report task_report;
    vf2_recovered_kill_osage_report kill_report;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    uint64_t body_instructions = 0u;
    uint64_t nested_calls = 0u;
    uint64_t nested_returns = 0u;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter = 0u;
    uint8_t instance = 0u;
    int interpreted_task = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->registers[29] != registry_address ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip == VF2_TASK_CAMERA_ENTRY) {
        return hybrid_execute_camera_task(
            machine, cpu, registry_address, report
        );
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(&task_report, 0, sizeof(task_report));
    memset(&kill_report, 0, sizeof(kill_report));
    local_report.entry_address = cpu->ip;
    local_report.registry_address = registry_address;
    start_instructions = cpu->executed_instructions;
    start_calls = cpu->procedure_calls;
    start_returns = cpu->procedure_returns;

    switch (cpu->ip) {
    case VF2_TASK_GAME_INFO_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_GAME_INFO;
        status = hybrid_game_info_interpreter_needed(
            machine, &interpreted_task
        );
        if (status == VF2_OK && interpreted_task) {
            status = hybrid_execute_game_info_bit31_native(
                machine, cpu, &task_report
            );
            if (status == VF2_OK) {
                task_report.registry_address = registry_address;
            }
        } else if (status == VF2_OK) {
            status = vf2_recovered_task_game_info_first_dispatch(
                machine, registry_address, &task_report
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500804), &fighter0
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500808), &fighter1
                );
            }
            if (status == VF2_OK) {
                cpu->registers[23] = fighter1;
                cpu->registers[24] = fighter0;
                body_instructions = UINT64_C(18);
            }
        }
        break;

    case VF2_PLAYER_TASK_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_PLAYER;
        interpreted_task = 1;
        status = hybrid_execute_interpreted_task(
            machine, cpu, registry_address, VF2_PLAYER_TASK_ENTRY,
            &task_report
        );
        break;

    case VF2_TASK_USER_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_USER;
        status = vf2_recovered_task_user_execute(
            machine, registry_address, &task_report
        );
        break;

    case VF2_TASK_SOUND_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_SOUND;
        status = vf2_recovered_task_sound_initialize(
            machine, registry_address, &task_report
        );
        if (status == VF2_OK) {
            body_instructions = UINT64_C(14);
        }
        break;

    case VF2_TASK_KILL_OSAGE_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_KILL_OSAGE;
        status = vf2_recovered_task_kill_osage_execute(
            machine, registry_address, &kill_report
        );
        if (status == VF2_OK) {
            cpu->registers[22] = kill_report.elapsed_ticks;
            cpu->registers[23] = kill_report.second_registry_address;
            body_instructions = UINT64_C(35);
            nested_calls = UINT64_C(2);
            nested_returns = UINT64_C(2);
            local_report.task_bytes_written =
                kill_report.flag_words_written * sizeof(uint32_t);
            local_report.global_bytes_written =
                kill_report.records_marked_for_kill * sizeof(uint32_t);
        }
        break;

    case VF2_TASK_OSAGE_ENTRY:
        status = vf2_model2a_read(
            machine, registry_address + UINT32_C(4),
            &instance, sizeof(instance)
        );
        if (status == VF2_OK && instance > 1u) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            local_report.kind = instance == 0u
                ? VF2_HYBRID_TASK_OSAGE0 : VF2_HYBRID_TASK_OSAGE1;
            status = vf2_recovered_task_osage_first_dispatch(
                machine, registry_address, &task_report
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry_address + UINT32_C(0x40), &fighter
            );
        }
        if (status == VF2_OK) {
            cpu->registers[23] = fighter;
            body_instructions = instance == 0u
                ? UINT64_C(18) : UINT64_C(17);
        }
        break;

    default:
        status = VF2_ERROR_UNSUPPORTED;
        break;
    }

    if (status == VF2_OK && local_report.kind != VF2_HYBRID_TASK_KILL_OSAGE) {
        local_report.task_bytes_written = task_report.bytes_written;
        local_report.global_bytes_written = task_report.global_bytes_written;
    }
    if (status == VF2_OK && !interpreted_task) {
        status = hybrid_complete_procedure(
            machine, cpu, body_instructions, nested_calls, nested_returns
        );
    }
    if (status == VF2_OK && cpu->ip != VF2_TASK_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        local_report.exit_address = cpu->ip;
        local_report.recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        local_report.recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        local_report.recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}

const char *vf2_hybrid_task_kind_name(vf2_hybrid_task_kind kind)
{
    switch (kind) {
    case VF2_HYBRID_TASK_GAME_INFO:
        return "fa_game_info";
    case VF2_HYBRID_TASK_CAMERA:
        return "fa_camera";
    case VF2_HYBRID_TASK_USER:
        return "fa_user";
    case VF2_HYBRID_TASK_SOUND:
        return "fa_sound";
    case VF2_HYBRID_TASK_KILL_OSAGE:
        return "fa_kill_osage";
    case VF2_HYBRID_TASK_OSAGE0:
        return "fa_osage0";
    case VF2_HYBRID_TASK_OSAGE1:
        return "fa_osage1";
    case VF2_HYBRID_TASK_PLAYER:
        return "fa_player";
    case VF2_HYBRID_TASK_NONE:
    default:
        return "none";
    }
}

const char *vf2_hybrid_block_kind_name(vf2_hybrid_block_kind kind)
{
    switch (kind) {
    case VF2_HYBRID_BLOCK_CAMERA_INITIALIZE:
        return "camera-initialize";
    case VF2_HYBRID_BLOCK_CAMERA_UPDATE:
        return "camera-update";
    case VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE:
        return "camera-post-update";
    case VF2_HYBRID_BLOCK_NONE:
    default:
        return "none";
    }
}

vf2_status vf2_hybrid_first_dispatch_scheduler_finish(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t current_task_index,
    uint32_t current_registry_address,
    vf2_hybrid_scheduler_finish_report *report
)
{
    static const uint16_t final_tiles[18] = {
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8020),
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8020),
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8020),
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8045),
        UINT16_C(0x8058), UINT16_C(0x8041), UINT16_C(0x8044),
        UINT16_C(0x0020), UINT16_C(0x0000), UINT16_C(0x0000)
    };
    enum {
        LAST_TASK_INDEX = 27u,
        INACTIVE_TASK_INDEX = 28u
    };
    const uint32_t inactive_registry = UINT32_C(0x00516400);
    const uint32_t end_registry = UINT32_C(0x00516480);
    const uint32_t current_scratch =
        VF2_SCHEDULER_SCRATCH_BASE + LAST_TASK_INDEX * VF2_SCHEDULER_SCRATCH_STRIDE;
    const uint32_t inactive_scratch =
        VF2_SCHEDULER_SCRATCH_BASE + INACTIVE_TASK_INDEX * VF2_SCHEDULER_SCRATCH_STRIDE;
    uint8_t task_name[VF2_SCHEDULER_TASK_NAME_SIZE];
    uint8_t tile_bytes[sizeof(final_tiles)];
    uint8_t input_flags = 0u;
    uint32_t input_pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t inactive_flags = 0u;
    uint32_t scratch_count = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;
    vf2_hybrid_scheduler_finish_report local_report;

    if (machine == NULL || cpu == NULL ||
        current_task_index != LAST_TASK_INDEX ||
        current_registry_address != UINT32_C(0x00516180) ||
        cpu->ip != VF2_TASK_SCHEDULER_RETURN ||
        cpu->local_frame_depth != 1u ||
        cpu->registers[29] != current_registry_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(task_name, 0, sizeof(task_name));
    status = vf2_model2a_read_u32(
        machine, VF2_SCHEDULER_TASK_COUNT_ADDRESS, &task_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_INPUT_POINTER, &input_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, input_pointer + UINT32_C(0xde),
            &input_flags, sizeof(input_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), &timer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(8), &timer2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry_address + UINT32_C(0x38), &threshold
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, inactive_registry, &inactive_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(8), &scratch_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_SCHEDULER_TASK_NAME_TABLE +
                INACTIVE_TASK_INDEX * VF2_SCHEDULER_TASK_NAME_STRIDE,
            task_name, VF2_SCHEDULER_TASK_NAME_TEXT_SIZE
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (task_count != 29u ||
        (runtime_flags & ((UINT32_C(1) << 5u) | (UINT32_C(1) << 9u))) != 0u ||
        (input_flags & (UINT8_C(1) << 2u)) != 0u ||
        (timer1 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        (timer2 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        threshold != 0u ||
        (inactive_flags & UINT32_C(0x80000000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch_count;
    status = vf2_model2a_write_u32(
        machine, current_scratch + UINT32_C(8), scratch_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, current_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, inactive_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SCHEDULER_CURRENT_INDEX, INACTIVE_TASK_INDEX
        );
    }
    task_name[VF2_SCHEDULER_TASK_NAME_TEXT_SIZE] = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_BUFFER, task_name, sizeof(task_name)
        );
    }
    for (index = 0u; index < sizeof(final_tiles) / sizeof(final_tiles[0]); ++index) {
        tile_bytes[index * 2u] = (uint8_t)final_tiles[index];
        tile_bytes[index * 2u + 1u] = (uint8_t)(final_tiles[index] >> 8u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_FORMAT,
            tile_bytes, sizeof(tile_bytes)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), VF2_SCHEDULER_TIMER_MASK
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[16] = UINT32_C(0x00444158);
    cpu->registers[25] = UINT32_C(0x010004dc);
    cpu->registers[29] = end_registry;
    cpu->executed_instructions += UINT64_C(280);
    cpu->procedure_calls += UINT64_C(4);
    cpu->procedure_returns += UINT64_C(4);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    if (cpu->ip != UINT32_C(0x0000a014)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    local_report.current_task_index = LAST_TASK_INDEX;
    local_report.inactive_descriptors_scanned = 1u;
    local_report.final_task_index = INACTIVE_TASK_INDEX;
    local_report.current_registry_address = current_registry_address;
    local_report.inactive_registry_address = inactive_registry;
    local_report.end_registry_address = end_registry;
    local_report.continuation_address = cpu->ip;
    local_report.recovered_instruction_count = UINT64_C(281);
    local_report.recovered_procedure_calls = UINT64_C(4);
    local_report.recovered_procedure_returns = UINT64_C(5);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
