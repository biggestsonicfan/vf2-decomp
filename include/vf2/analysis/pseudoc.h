#ifndef VF2_ANALYSIS_PSEUDOC_H
#define VF2_ANALYSIS_PSEUDOC_H

#include <stdio.h>
#include <stdint.h>

#include "vf2/status.h"

struct vf2_i960_analysis;

vf2_status vf2_i960_write_function_pseudoc(
    const struct vf2_i960_analysis *analysis,
    uint32_t function_address,
    FILE *output
);

vf2_status vf2_i960_write_all_pseudoc(
    const struct vf2_i960_analysis *analysis,
    const char *output_directory
);

#endif
