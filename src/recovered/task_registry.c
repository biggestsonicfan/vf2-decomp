#include "vf2/recovered.h"

#include <string.h>

#define VF2_TASK_REGISTRY_BASE UINT32_C(0x00510000)
#define VF2_TASK_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_TASK_SCRATCH_STRIDE UINT32_C(0x20)

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, 1u);
}

static vf2_status clear_bytes(vf2_model2a *machine, uint32_t address, size_t count)
{
    static const uint8_t zeros[VF2_TASK_SCRATCH_STRIDE] = {0u};
    size_t remaining = count;
    while (remaining != 0u) {
        const size_t chunk = remaining < sizeof(zeros) ? remaining : sizeof(zeros);
        vf2_status status = vf2_model2a_write(machine, address, zeros, chunk);
        if (status != VF2_OK) {
            return status;
        }
        address += (uint32_t)chunk;
        remaining -= chunk;
    }
    return VF2_OK;
}

vf2_status vf2_recovered_task_registry_initialize(
    vf2_model2a *machine,
    const vf2_task_catalog *catalog,
    vf2_recovered_task_registry_report *report
)
{
    uint32_t registry = VF2_TASK_REGISTRY_BASE;
    uint32_t scratch = VF2_TASK_SCRATCH_BASE;
    size_t index = 0u;
    vf2_recovered_task_registry_report local_report;

    if (machine == NULL || catalog == NULL || catalog->tasks == NULL ||
        catalog->count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.source_table_start = catalog->table_start;
    local_report.source_table_end = catalog->table_end;
    local_report.registry_start = registry;
    local_report.scratch_start = scratch;

    for (index = 0u; index < catalog->count; ++index) {
        const vf2_task_descriptor *task = &catalog->tasks[index];
        const uint32_t scheduler_value = task->scheduler_slot == 0u
            ? 0u
            : task->scheduler_slot * UINT32_C(25) + UINT32_C(6);
        vf2_status status = vf2_model2a_write_u32(machine, registry + 0x00u, task->flags);
        if (status == VF2_OK) {
            status = write_u8(machine, registry + 0x04u, (uint8_t)task->instance);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, registry + 0x08u, task->stack_size);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, registry + 0x0cu, task->entry_point);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, registry + 0x38u, scheduler_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, task->state_address, registry);
        }
        if (status == VF2_OK) {
            status = clear_bytes(machine, scratch, VF2_TASK_SCRATCH_STRIDE);
        }
        if (status != VF2_OK) {
            return status;
        }
        registry += task->stack_size;
        scratch += VF2_TASK_SCRATCH_STRIDE;
        ++local_report.state_pointers_written;
        local_report.scratch_bytes_cleared += VF2_TASK_SCRATCH_STRIDE;
    }

    local_report.task_count = catalog->count;
    local_report.registry_end = registry;
    local_report.scratch_end = scratch;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
