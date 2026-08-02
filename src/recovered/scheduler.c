#include "vf2/recovered.h"

#include <string.h>

#define VF2_TASK_REGISTRY_BASE UINT32_C(0x00510000)
#define VF2_TASK_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_TASK_SCRATCH_STRIDE UINT32_C(0x20)
#define VF2_TASK_RUNNABLE_MASK UINT32_C(0x80000000)

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
