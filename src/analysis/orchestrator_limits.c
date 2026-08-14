#include "vf2/analysis/orchestrator_limits.h"

#define VF2_ORCHESTRATOR_RUNTIME_SPECIAL_BIT (UINT32_C(1) << 16u)
#define VF2_ORCHESTRATOR_MODE_MASK_A UINT32_C(0x000000c0)
#define VF2_ORCHESTRATOR_MODE_MASK_B UINT32_C(0x0000c000)
#define VF2_ORCHESTRATOR_MODE_MASK_C UINT32_C(0x0000000c)
#define VF2_ORCHESTRATOR_DEFAULT_LOW UINT32_C(0x00003e80)
#define VF2_ORCHESTRATOR_DEFAULT_HIGH UINT32_C(0x00004e20)
#define VF2_ORCHESTRATOR_DEFAULT_INSTRUCTIONS UINT64_C(22)

static int mode_selects_alternate_branch(uint8_t display_mode)
{
    uint32_t mode_bit = 0u;

    if (display_mode >= UINT8_C(32)) {
        return 1;
    }
    if (display_mode == UINT8_C(9) ||
        display_mode == UINT8_C(12) ||
        display_mode == UINT8_C(13)) {
        return 1;
    }

    mode_bit = UINT32_C(1) << display_mode;
    return (mode_bit & (
        VF2_ORCHESTRATOR_MODE_MASK_A |
        VF2_ORCHESTRATOR_MODE_MASK_B |
        VF2_ORCHESTRATOR_MODE_MASK_C
    )) != 0u;
}

vf2_status vf2_orchestrator_select_default_limits(
    uint32_t runtime_flags,
    uint8_t display_mode,
    uint32_t *lower_limit,
    uint32_t *upper_limit
)
{
    if (lower_limit == NULL || upper_limit == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((runtime_flags & VF2_ORCHESTRATOR_RUNTIME_SPECIAL_BIT) != 0u ||
        mode_selects_alternate_branch(display_mode)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *lower_limit = VF2_ORCHESTRATOR_DEFAULT_LOW;
    *upper_limit = VF2_ORCHESTRATOR_DEFAULT_HIGH;
    return VF2_OK;
}

vf2_status vf2_orchestrator_apply_default_limits(
    vf2_model2a *machine,
    vf2_orchestrator_limits_report *report
)
{
    vf2_orchestrator_limits_report local_report = {0};
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    local_report.entry_address = VF2_ORCHESTRATOR_LIMITS_ENTRY;
    status = vf2_model2a_read_u32(
        machine,
        VF2_ORCHESTRATOR_RUNTIME_FLAGS,
        &local_report.runtime_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &local_report.display_mode,
            sizeof(local_report.display_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_orchestrator_select_default_limits(
            local_report.runtime_flags,
            local_report.display_mode,
            &local_report.lower_limit,
            &local_report.upper_limit
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_ORCHESTRATOR_LIMIT_LOW,
            local_report.lower_limit
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_ORCHESTRATOR_LIMIT_HIGH,
            local_report.upper_limit
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.interpreted_instruction_equivalent =
        VF2_ORCHESTRATOR_DEFAULT_INSTRUCTIONS;
    local_report.bytes_written = 2u * sizeof(uint32_t);
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

#include "../recovered/texture_bridge_condition.c"
