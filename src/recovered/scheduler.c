#include "vf2/recovered.h"
#include "vf2/hybrid.h"

#include <string.h>

#define VF2_TASK_REGISTRY_BASE UINT32_C(0x00510000)
#define VF2_TASK_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_TASK_SCRATCH_STRIDE UINT32_C(0x20)
#define VF2_TASK_RUNNABLE_MASK UINT32_C(0x80000000)

#define VF2_NATIVE_SECOND_CALL_SITE UINT32_C(0x0000a010)
#define VF2_NATIVE_SECOND_ENTRY UINT32_C(0x00010d54)
#define VF2_NATIVE_SCHEDULER_RETURN UINT32_C(0x00010dcc)
#define VF2_NATIVE_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_NATIVE_INITIALIZER_ENTRY UINT32_C(0x000221cc)
#define VF2_NATIVE_INITIALIZER_RECURRING UINT32_C(0x000221e8)
#define VF2_NATIVE_INITIALIZER_INDEX 10u
#define VF2_NATIVE_GAME_INFO_INDEX 13u
#define VF2_NATIVE_INITIALIZER_REGISTRY UINT32_C(0x00514980)
#define VF2_NATIVE_GAME_INFO_REGISTRY UINT32_C(0x00515200)
#define VF2_NATIVE_CURRENT_INDEX UINT32_C(0x00500038)
#define VF2_NATIVE_READY_FLAGS UINT32_C(0x00500068)
#define VF2_NATIVE_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_NATIVE_TASK_COUNT UINT32_C(0x00011d94)
#define VF2_NATIVE_TIMER1 UINT32_C(0x00f00004)
#define VF2_NATIVE_TIMER2 UINT32_C(0x00f00008)
#define VF2_NATIVE_TIMER_MASK UINT32_C(0x000fffff)
#define VF2_NATIVE_GEOMETRY_STATUS UINT32_C(0x00800070)
#define VF2_NATIVE_GEOMETRY_COMMAND UINT32_C(0x00804000)
#define VF2_NATIVE_SPECIAL_INSTRUCTIONS UINT64_C(254)
#define VF2_NATIVE_SPECIAL_CALLS UINT64_C(5)
#define VF2_NATIVE_SPECIAL_RETURNS UINT64_C(3)

vf2_status vf2_recovered_scheduler_plan(
    const vf2_model2a *machine,
    const vf2_task_catalog *catalog,
    vf2_recovered_scheduler_report *report
)
{
    vf2_recovered_scheduler_report local_report;
    uint32_t registry = VF2_TASK_REGISTRY_BASE;
    uint32_t scratch = VF2_TASK_SCRATCH_BASE;
    size_t index = 0u;

    if (machine == NULL || catalog == NULL || catalog->tasks == NULL ||
        catalog->count == 0u || catalog->count > VF2_RECOVERED_SCHEDULER_MAX_TASKS) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.registry_start = registry;
    local_report.scratch_start = scratch;

    for (index = 0u; index < catalog->count; ++index) {
        const vf2_task_descriptor *task = &catalog->tasks[index];
        uint32_t flags = 0u;
        uint32_t stack_size = 0u;
        uint32_t entry_point = 0u;
        vf2_status status = vf2_model2a_read_u32(machine, registry + 0x00u, &flags);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, registry + 0x08u, &stack_size);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, registry + 0x0cu, &entry_point);
        }
        if (status != VF2_OK) {
            return status;
        }
        if (stack_size != task->stack_size || entry_point != task->entry_point ||
            stack_size == 0u || (stack_size & UINT32_C(0x1f)) != 0u) {
            return VF2_ERROR_BAD_SIZE;
        }
        if ((flags & VF2_TASK_RUNNABLE_MASK) != 0u) {
            const size_t runnable = local_report.runnable_count;
            local_report.runnable_task_indices[runnable] = index;
            local_report.runnable_registry_addresses[runnable] = registry;
            local_report.runnable_entry_points[runnable] = entry_point;
            ++local_report.runnable_count;
        }
        registry += stack_size;
        scratch += VF2_TASK_SCRATCH_STRIDE;
    }

    local_report.descriptor_count = catalog->count;
    local_report.registry_end = registry;
    local_report.scratch_end = scratch;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

static vf2_status first_runnable_descriptor(
    const vf2_model2a *machine,
    uint32_t task_count,
    size_t *selected_index,
    uint32_t *selected_registry,
    uint32_t *selected_entry
)
{
    uint32_t registry = VF2_TASK_REGISTRY_BASE;
    size_t index = 0u;

    if (machine == NULL || selected_index == NULL ||
        selected_registry == NULL || selected_entry == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0u; index < task_count; ++index) {
        uint32_t flags = 0u;
        uint32_t stride = 0u;
        vf2_status status = vf2_model2a_read_u32(machine, registry, &flags);

        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(8), &stride
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (stride == 0u || (stride & UINT32_C(0x1f)) != 0u) {
            return VF2_ERROR_BAD_SIZE;
        }
        if ((flags & VF2_TASK_RUNNABLE_MASK) != 0u) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(0x0c), selected_entry
            );
            if (status != VF2_OK) {
                return status;
            }
            *selected_index = index;
            *selected_registry = registry;
            return VF2_OK;
        }
        registry += stride;
    }

    *selected_index = task_count;
    *selected_registry = registry;
    *selected_entry = 0u;
    return VF2_OK;
}

static void set_equal_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
}

static vf2_status execute_initializer_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry
)
{
    vf2_status status = VF2_OK;

    cpu->registers[15] = VF2_NATIVE_INITIALIZER_RECURRING;
    status = vf2_model2a_write_u32(
        machine, registry + UINT32_C(0x0c), cpu->registers[15]
    );
    cpu->registers[15] = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x90), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x13c), 0u
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status execute_initializer_scheduler_corridor(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
)
{
    vf2_hybrid_second_scheduler_report local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t ready_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t registry = VF2_TASK_REGISTRY_BASE;
    uint32_t scratch = VF2_TASK_SCRATCH_BASE;
    uint32_t selected_entry = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_NATIVE_SECOND_CALL_SITE ||
        cpu->local_frame_depth > 1u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    status = vf2_model2a_read_u32(machine, VF2_NATIVE_READY_FLAGS, &ready_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_NATIVE_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TASK_COUNT, &task_count);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER1, &timer1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER2, &timer2);
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((ready_flags & (UINT32_C(1) << 16u)) != 0u ||
        task_count != UINT32_C(29) ||
        (runtime_flags & (UINT32_C(1) << 9u)) == 0u ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        (timer2 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_SECOND_ENTRY, VF2_NATIVE_SECOND_CALL_SITE + UINT32_C(4)
    );
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_write_u32(machine, VF2_NATIVE_GEOMETRY_STATUS, 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_NATIVE_GEOMETRY_COMMAND, 3u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_NATIVE_GEOMETRY_STATUS, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_NATIVE_GEOMETRY_COMMAND, 1u);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->procedure_calls += UINT64_C(2);
    cpu->procedure_returns += UINT64_C(2);

    for (index = 0u; index <= VF2_NATIVE_INITIALIZER_INDEX; ++index) {
        uint32_t flags = 0u;
        uint32_t stride = 0u;

        status = vf2_model2a_write_u32(
            machine, VF2_NATIVE_CURRENT_INDEX, (uint32_t)index
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, VF2_NATIVE_TIMER1, VF2_NATIVE_TIMER_MASK
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, registry, &flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(8), &stride
            );
        }
        if (status != VF2_OK || stride == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        if (index == VF2_NATIVE_INITIALIZER_INDEX) {
            if ((flags & VF2_TASK_RUNNABLE_MASK) == 0u ||
                registry != VF2_NATIVE_INITIALIZER_REGISTRY) {
                return VF2_ERROR_UNSUPPORTED;
            }
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(0x0c), &selected_entry
            );
            if (status != VF2_OK || selected_entry != VF2_NATIVE_INITIALIZER_ENTRY) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            break;
        }

        if ((flags & VF2_TASK_RUNNABLE_MASK) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write_u32(
            machine,
            scratch + UINT32_C(0x10),
            VF2_NATIVE_TIMER_MASK - (timer2 & VF2_NATIVE_TIMER_MASK)
        );
        if (status != VF2_OK) {
            return status;
        }
        registry += stride;
        scratch += VF2_TASK_SCRATCH_STRIDE;
    }

    memset(&cpu->registers[2], 0, 14u * sizeof(cpu->registers[0]));
    cpu->registers[0] = UINT32_C(0x005ff500);
    cpu->registers[2] = UINT32_C(0x00010d64);
    cpu->registers[3] = task_count;
    cpu->registers[4] = VF2_NATIVE_INITIALIZER_ENTRY;
    cpu->registers[8] = VF2_NATIVE_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = scratch;
    cpu->registers[11] = VF2_NATIVE_INITIALIZER_INDEX;
    cpu->registers[13] = VF2_TASK_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] = timer2;
    cpu->registers[16] = UINT32_C(1);
    cpu->registers[29] = registry;
    set_equal_condition(cpu);

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_INITIALIZER_ENTRY, VF2_NATIVE_SCHEDULER_RETURN
    );
    if (status == VF2_OK) {
        status = execute_initializer_task(machine, cpu, registry);
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_SCHEDULER_RETURN ||
        cpu->local_frame_depth != 1u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    {
        uint32_t scratch0 = 0u;
        uint32_t scratch1 = 0u;
        uint32_t scratch2 = 0u;
        uint32_t scratch3 = 0u;
        uint32_t next_registry = registry;
        uint32_t next_scratch = scratch;
        size_t next_index = VF2_NATIVE_INITIALIZER_INDEX;
        size_t scanned = 0u;

        status = vf2_model2a_read_u32(machine, scratch, &scratch0);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, scratch + UINT32_C(4), &scratch1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, scratch + UINT32_C(8), &scratch2
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, scratch + UINT32_C(12), &scratch3
            );
        }
        if (status == VF2_OK) {
            ++scratch2;
            status = vf2_model2a_write_u32(
                machine, scratch + UINT32_C(8), scratch2
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, scratch + UINT32_C(0x10), 0u
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        while (next_index + 1u < task_count) {
            uint32_t stride = 0u;
            uint32_t flags = 0u;

            status = vf2_model2a_read_u32(
                machine, next_registry + UINT32_C(8), &stride
            );
            if (status != VF2_OK || stride == 0u) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            next_registry += stride;
            next_scratch += VF2_TASK_SCRATCH_STRIDE;
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
            if ((flags & VF2_TASK_RUNNABLE_MASK) != 0u) {
                status = vf2_model2a_read_u32(
                    machine, next_registry + UINT32_C(0x0c), &selected_entry
                );
                break;
            }
            status = vf2_model2a_write_u32(
                machine, next_scratch + UINT32_C(0x10), 0u
            );
            if (status != VF2_OK) {
                return status;
            }
        }

        if (status != VF2_OK || scanned != 3u ||
            next_index != VF2_NATIVE_GAME_INFO_INDEX ||
            next_registry != VF2_NATIVE_GAME_INFO_REGISTRY ||
            selected_entry != VF2_NATIVE_GAME_INFO_ENTRY) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        cpu->registers[3] = task_count;
        cpu->registers[4] = selected_entry;
        cpu->registers[5] = scratch1;
        cpu->registers[6] = scratch2;
        cpu->registers[7] = scratch3;
        cpu->registers[8] = VF2_NATIVE_TIMER_MASK;
        cpu->registers[9] = runtime_flags;
        cpu->registers[10] = next_scratch;
        cpu->registers[11] = (uint32_t)next_index;
        cpu->registers[12] = 0u;
        cpu->registers[13] = VF2_TASK_SCRATCH_STRIDE;
        cpu->registers[14] = timer1;
        cpu->registers[15] = timer2;
        cpu->registers[29] = next_registry;
        set_equal_condition(cpu);

        status = vf2_i960_cpu_enter_procedure(
            cpu, selected_entry, VF2_NATIVE_SCHEDULER_RETURN
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    if (cpu->procedure_calls - start_calls != VF2_NATIVE_SPECIAL_CALLS ||
        cpu->procedure_returns - start_returns != VF2_NATIVE_SPECIAL_RETURNS) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->executed_instructions = start_instructions + VF2_NATIVE_SPECIAL_INSTRUCTIONS;

    local_report.descriptors_scanned = UINT64_C(14);
    local_report.inactive_descriptors_scanned = UINT64_C(12);
    local_report.selected_task_index = VF2_NATIVE_GAME_INFO_INDEX;
    local_report.registry_start = VF2_TASK_REGISTRY_BASE;
    local_report.selected_registry_address = VF2_NATIVE_GAME_INFO_REGISTRY;
    local_report.selected_entry_address = VF2_NATIVE_GAME_INFO_ENTRY;
    local_report.scheduler_entry_address = VF2_NATIVE_SECOND_ENTRY;
    local_report.recovered_instruction_count = VF2_NATIVE_SPECIAL_INSTRUCTIONS;
    local_report.recovered_procedure_calls = VF2_NATIVE_SPECIAL_CALLS;
    local_report.recovered_procedure_returns = VF2_NATIVE_SPECIAL_RETURNS;
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_native_second_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
)
{
    uint32_t task_count = 0u;
    size_t selected_index = 0u;
    uint32_t selected_registry = 0u;
    uint32_t selected_entry = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_NATIVE_SECOND_CALL_SITE) {
        return vf2_hybrid_second_scheduler_enter(machine, cpu, report);
    }

    status = vf2_model2a_read_u32(machine, VF2_NATIVE_TASK_COUNT, &task_count);
    if (status == VF2_OK) {
        status = first_runnable_descriptor(
            machine,
            task_count,
            &selected_index,
            &selected_registry,
            &selected_entry
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (task_count == UINT32_C(29) &&
        selected_index == VF2_NATIVE_INITIALIZER_INDEX &&
        selected_registry == VF2_NATIVE_INITIALIZER_REGISTRY &&
        selected_entry == VF2_NATIVE_INITIALIZER_ENTRY) {
        return execute_initializer_scheduler_corridor(machine, cpu, report);
    }
    return vf2_hybrid_second_scheduler_enter(machine, cpu, report);
}
