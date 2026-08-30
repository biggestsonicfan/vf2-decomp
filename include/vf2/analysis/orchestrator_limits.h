#ifndef VF2_ANALYSIS_ORCHESTRATOR_LIMITS_H
#define VF2_ANALYSIS_ORCHESTRATOR_LIMITS_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/model2a.h"
#include "vf2/status.h"

enum {
    VF2_ORCHESTRATOR_LIMITS_ENTRY = 0x0004bfe0u,
    VF2_ORCHESTRATOR_RUNTIME_FLAGS = 0x00500068u,
    VF2_ORCHESTRATOR_DISPLAY_MODE = 0x0050002bu,
    VF2_ORCHESTRATOR_LIMIT_LOW = 0x00550004u,
    VF2_ORCHESTRATOR_LIMIT_HIGH = 0x00550008u,
    VF2_ORCHESTRATOR_FIELD_50064 = 0x00500064u,
    VF2_ORCHESTRATOR_FIELD_50031 = 0x00500031u
};

enum {
    VF2_ORCHESTRATOR_LIMIT_3E80 = 0x00003e80u,
    VF2_ORCHESTRATOR_LIMIT_4E20 = 0x00004e20u,
    VF2_ORCHESTRATOR_LIMIT_4330 = 0x00004330u,
    VF2_ORCHESTRATOR_LIMIT_12A8 = 0x000012a8u,
    VF2_ORCHESTRATOR_LIMIT_32C8 = 0x000032c8u
};

typedef struct vf2_orchestrator_limits_report {
    uint32_t entry_address;
    uint32_t runtime_flags;
    uint8_t display_mode;
    uint32_t lower_limit;
    uint32_t upper_limit;
    uint64_t interpreted_instruction_equivalent;
    size_t bytes_written;
} vf2_orchestrator_limits_report;

vf2_status vf2_orchestrator_select_default_limits(
    uint32_t runtime_flags,
    uint8_t display_mode,
    uint32_t *lower_limit,
    uint32_t *upper_limit
);

vf2_status vf2_orchestrator_select_limits(
    uint32_t runtime_flags,
    uint8_t display_mode,
    uint8_t field_50064,
    uint8_t field_50031,
    uint32_t *lower_limit,
    uint32_t *upper_limit
);

vf2_status vf2_orchestrator_apply_default_limits(
    vf2_model2a *machine,
    vf2_orchestrator_limits_report *report
);

#ifdef VF2_TEXTURE_BRIDGE_INTERNAL_H
static inline vf2_status vf2_texture_default_limits_return(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine
)
{
    uint8_t display_mode = 0u;
    vf2_status status = VF2_OK;

    if (cpu == NULL || machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip == VF2_ORCHESTRATOR_LIMITS_ENTRY) {
        status = vf2_model2a_read(
            machine,
            VF2_ORCHESTRATOR_DISPLAY_MODE,
            &display_mode,
            sizeof(display_mode)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->arithmetic_control &= ~UINT32_C(7);
        if (display_mode < UINT8_C(9)) {
            cpu->arithmetic_control |= UINT32_C(4);
            cpu->compare_result = VF2_I960_COMPARE_LESS;
        } else if (display_mode > UINT8_C(9)) {
            cpu->arithmetic_control |= UINT32_C(1);
            cpu->compare_result = VF2_I960_COMPARE_GREATER;
        } else {
            cpu->arithmetic_control |= UINT32_C(2);
            cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        }
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

#define vf2_i960_cpu_return_procedure vf2_texture_default_limits_return
#endif

#endif
