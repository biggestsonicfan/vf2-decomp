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
    VF2_ORCHESTRATOR_LIMIT_HIGH = 0x00550008u
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

/*
 * Evidence-bounded semantic candidate for the default branch class observed
 * at 0x0004bfe0. Alternate mode and runtime-flag branches remain unsupported.
 */
vf2_status vf2_orchestrator_select_default_limits(
    uint32_t runtime_flags,
    uint8_t display_mode,
    uint32_t *lower_limit,
    uint32_t *upper_limit
);

/*
 * Read the original work-RAM inputs, apply the accepted default decision and
 * write the two texture-limit words. This is an analysis candidate until the
 * live i960 path is differentially connected to it.
 */
vf2_status vf2_orchestrator_apply_default_limits(
    vf2_model2a *machine,
    vf2_orchestrator_limits_report *report
);

#ifdef VF2_TEXTURE_BRIDGE_INTERNAL_H
#if defined(__GNUC__) || defined(__clang__)
#pragma weak execute_texture_orchestrator_entry_gate
#endif

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

static inline vf2_status vf2_texture_bridge_enter_procedure(
    vf2_i960_cpu *cpu,
    uint32_t target,
    uint32_t return_address
)
{
    const vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu, target, return_address
    );

    if (status == VF2_OK &&
        target == UINT32_C(0x0004d2c0) &&
        return_address == UINT32_C(0x0004bd5c)) {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
    }
    return status;
}

#define vf2_i960_cpu_return_procedure vf2_texture_default_limits_return
#define vf2_i960_cpu_enter_procedure vf2_texture_bridge_enter_procedure
#endif

#endif
