#ifndef VF2_ANALYSIS_SYMBOLS_H
#define VF2_ANALYSIS_SYMBOLS_H

#include <stdint.h>

#include "vf2/status.h"

struct vf2_i960_analysis;

vf2_status vf2_i960_apply_symbol_overlays(
    struct vf2_i960_analysis *analysis,
    const char *directory
);

const char *vf2_i960_function_name(
    const struct vf2_i960_analysis *analysis,
    uint32_t address
);

#endif
