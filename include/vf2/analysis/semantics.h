#ifndef VF2_ANALYSIS_SEMANTICS_H
#define VF2_ANALYSIS_SEMANTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

struct vf2_i960_analysis;

typedef enum vf2_value_kind {
    VF2_VALUE_UNKNOWN = 0,
    VF2_VALUE_CONSTANT,
    VF2_VALUE_STACK_RELATIVE,
    VF2_VALUE_ARGUMENT,
    VF2_VALUE_TABLE_LOOKUP
} vf2_value_kind;

typedef struct vf2_abstract_value {
    vf2_value_kind kind;
    int64_t value;
    uint32_t table_base;
    uint8_t table_scale;
    uint8_t argument_register;
} vf2_abstract_value;

typedef struct vf2_constant_fact {
    uint32_t address;
    uint8_t reg;
    vf2_abstract_value value;
} vf2_constant_fact;

typedef enum vf2_indirect_target_kind {
    VF2_INDIRECT_BRANCH = 0,
    VF2_INDIRECT_CALL,
    VF2_INDIRECT_JUMP_TABLE
} vf2_indirect_target_kind;

typedef struct vf2_indirect_target {
    uint32_t source;
    uint32_t target;
    uint32_t table_base;
    vf2_indirect_target_kind kind;
    uint8_t confidence;
} vf2_indirect_target;

const char *vf2_value_kind_name(vf2_value_kind kind);
const char *vf2_indirect_target_kind_name(vf2_indirect_target_kind kind);

vf2_status vf2_i960_run_semantic_analysis(
    struct vf2_i960_analysis *analysis
);

#endif
