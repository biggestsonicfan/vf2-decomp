#include "vf2/analysis/orchestrator_limits.h"

#include <stdint.h>

#define VF2_ORCHESTRATOR_RUNTIME_SPECIAL_BIT (UINT32_C(1) << 16u)

static int mode_is_skip(uint8_t display_mode)
{
    uint32_t mod = (uint32_t)display_mode % UINT32_C(32);
    return mod == UINT32_C(2) || mod == UINT32_C(3);
}

static int mode_is_12a8(uint8_t display_mode)
{
    uint32_t mod = (uint32_t)display_mode % UINT32_C(32);
    return mod == UINT32_C(6) || mod == UINT32_C(7);
}

static int mode_is_32c8(uint8_t display_mode)
{
    uint32_t mod = (uint32_t)display_mode % UINT32_C(32);
    return mod == UINT32_C(14) || mod == UINT32_C(15);
}

vf2_status vf2_orchestrator_select_limits(
    uint32_t runtime_flags,
    uint8_t display_mode,
    uint8_t field_50064,
    uint8_t field_50031,
    uint32_t *lower_limit,
    uint32_t *upper_limit
)
{
    if (lower_limit == NULL || upper_limit == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((runtime_flags & VF2_ORCHESTRATOR_RUNTIME_SPECIAL_BIT) != 0u) {
        *lower_limit = 0u;
        *upper_limit = VF2_ORCHESTRATOR_LIMIT_4E20;
        return VF2_OK;
    }
    if (display_mode == UINT8_C(12) || display_mode == UINT8_C(13)) {
        *lower_limit = 0u;
        *upper_limit = VF2_ORCHESTRATOR_LIMIT_4E20;
        return VF2_OK;
    }
    if (mode_is_12a8(display_mode)) {
        *lower_limit = VF2_ORCHESTRATOR_LIMIT_12A8;
        *upper_limit = VF2_ORCHESTRATOR_LIMIT_4330;
        return VF2_OK;
    }
    if (mode_is_32c8(display_mode)) {
        *lower_limit = VF2_ORCHESTRATOR_LIMIT_32C8;
        *upper_limit = VF2_ORCHESTRATOR_LIMIT_4E20;
        return VF2_OK;
    }
    if (mode_is_skip(display_mode)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (display_mode == UINT8_C(9)) {
        if (field_50064 == UINT8_C(6) || field_50064 == UINT8_C(8)) {
            *lower_limit = VF2_ORCHESTRATOR_LIMIT_4330;
            *upper_limit = VF2_ORCHESTRATOR_LIMIT_4E20;
            return VF2_OK;
        }
        if (field_50031 < UINT8_C(8)) {
            *lower_limit = VF2_ORCHESTRATOR_LIMIT_4330;
            *upper_limit = 0u;
            return VF2_OK;
        }
        *lower_limit = VF2_ORCHESTRATOR_LIMIT_3E80;
        *upper_limit = VF2_ORCHESTRATOR_LIMIT_4E20;
        return VF2_OK;
    }
    *lower_limit = VF2_ORCHESTRATOR_LIMIT_3E80;
    *upper_limit = VF2_ORCHESTRATOR_LIMIT_4E20;
    return VF2_OK;
}

vf2_status vf2_orchestrator_select_default_limits(
    uint32_t runtime_flags,
    uint8_t display_mode,
    uint32_t *lower_limit,
    uint32_t *upper_limit
)
{
    /* Backwards-compatible wrapper: assume default extra fields are zero. */
    return vf2_orchestrator_select_limits(
        runtime_flags, display_mode, 0u, 0u, lower_limit, upper_limit
    );
}

vf2_status vf2_orchestrator_apply_default_limits(
    vf2_model2a *machine,
    vf2_orchestrator_limits_report *report
)
{
    vf2_orchestrator_limits_report local_report = {0};
    vf2_status status = VF2_OK;
    uint8_t field_50064 = 0u;
    uint8_t field_50031 = 0u;
    uint64_t instructions = 0u;

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
        /* Always read the secondary fields so the sweep is deterministic. */
        if (vf2_model2a_read(machine, VF2_ORCHESTRATOR_FIELD_50064, &field_50064, sizeof(field_50064)) != VF2_OK) {
            field_50064 = 0u;
        }
        if (vf2_model2a_read(machine, VF2_ORCHESTRATOR_FIELD_50031, &field_50031, sizeof(field_50031)) != VF2_OK) {
            field_50031 = 0u;
        }
        status = vf2_orchestrator_select_limits(
            local_report.runtime_flags,
            local_report.display_mode,
            field_50064,
            field_50031,
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

    /* Measured instruction equivalents for each branch (via vf2probe --until 0x4c11c) plus final ret. */
    if ((local_report.runtime_flags & VF2_ORCHESTRATOR_RUNTIME_SPECIAL_BIT) != 0u) {
        instructions = UINT64_C(8);
    } else if (local_report.display_mode == UINT8_C(12)) {
        instructions = UINT64_C(11);
    } else if (local_report.display_mode == UINT8_C(13)) {
        instructions = UINT64_C(13);
    } else if (mode_is_12a8(local_report.display_mode)) {
        instructions = UINT64_C(15);
    } else if (mode_is_32c8(local_report.display_mode)) {
        instructions = UINT64_C(18);
    } else if (local_report.display_mode == UINT8_C(9)) {
        if (field_50064 == UINT8_C(6) || field_50064 == UINT8_C(8)) {
            instructions = UINT64_C(25);
        } else if (field_50031 < UINT8_C(8)) {
            instructions = UINT64_C(31);
        } else {
            instructions = UINT64_C(31);
        }
    } else {
        instructions = UINT64_C(22);
    }

    local_report.interpreted_instruction_equivalent = instructions;
    local_report.bytes_written = 2u * sizeof(uint32_t);
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
