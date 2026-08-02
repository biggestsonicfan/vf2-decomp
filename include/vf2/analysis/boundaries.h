#ifndef VF2_ANALYSIS_BOUNDARIES_H
#define VF2_ANALYSIS_BOUNDARIES_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

struct vf2_i960_analysis;

typedef enum vf2_split_reason {
    VF2_SPLIT_OVERLAPPING_ENTRY = 0,
    VF2_SPLIT_TAIL_BRANCH
} vf2_split_reason;

typedef struct vf2_function_split {
    uint32_t source_function;
    uint32_t target_function;
    uint32_t instruction_address;
    vf2_split_reason reason;
} vf2_function_split;

const char *vf2_split_reason_name(vf2_split_reason reason);

vf2_status vf2_i960_detect_function_boundaries(
    struct vf2_i960_analysis *analysis
);

#endif
