#include "vf2/analysis/orchestrator_scan.h"

#include <string.h>

#define VF2_I960_CONDITION_MASK UINT32_C(7)
#define VF2_I960_CONDITION_EQUAL UINT32_C(2)
#define VF2_ORCHESTRATOR_SCAN_INSTRUCTIONS UINT64_C(43)

static vf2_status read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (machine == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status != VF2_OK) {
        return status;
    }
    *value = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
    return VF2_OK;
}

static void set_equal_condition(vf2_i960_cpu *cpu)
{
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~VF2_I960_CONDITION_MASK) |
        VF2_I960_CONDITION_EQUAL;
}

vf2_status vf2_orchestrator_scan_inactive_records(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_orchestrator_scan_report *report
)
{
    vf2_orchestrator_scan_report local_report;
    size_t records_scanned = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY;
    local_report.exit_address = VF2_ORCHESTRATOR_RECORD_SCAN_EXIT;
    local_report.first_record_address = VF2_ORCHESTRATOR_RECORD_START;
    local_report.end_record_address = VF2_ORCHESTRATOR_RECORD_END;

    cpu->registers[5] = VF2_ORCHESTRATOR_RECORD_START;
    cpu->registers[6] = VF2_ORCHESTRATOR_RECORD_END;

    for (;;) {
        uint16_t active_count = 0u;

        status = read_u16(
            machine,
            cpu->registers[5] + VF2_ORCHESTRATOR_RECORD_ACTIVE_OFFSET,
            &active_count
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] =
            (uint32_t)(int32_t)(int16_t)active_count;
        if (cpu->registers[3] != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }

        cpu->registers[5] += VF2_ORCHESTRATOR_RECORD_STRIDE;
        ++records_scanned;
        if (cpu->registers[5] >= cpu->registers[6]) {
            break;
        }
    }

    if (records_scanned != VF2_ORCHESTRATOR_RECORD_COUNT ||
        cpu->registers[5] != VF2_ORCHESTRATOR_RECORD_END) {
        return VF2_ERROR_UNSUPPORTED;
    }

    set_equal_condition(cpu);
    cpu->ip = VF2_ORCHESTRATOR_RECORD_SCAN_EXIT;
    cpu->executed_instructions += VF2_ORCHESTRATOR_SCAN_INSTRUCTIONS;

    local_report.records_scanned = records_scanned;
    local_report.recovered_instruction_count =
        VF2_ORCHESTRATOR_SCAN_INSTRUCTIONS;
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
