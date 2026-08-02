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

#endif
