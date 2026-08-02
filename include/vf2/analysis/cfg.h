#ifndef VF2_ANALYSIS_CFG_H
#define VF2_ANALYSIS_CFG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vf2/analysis/image_map.h"
#include "vf2/analysis/xref.h"
#include "vf2/analysis/semantics.h"
#include "vf2/analysis/boundaries.h"
#include "vf2/status.h"

#define VF2_MAX_BLOCK_SUCCESSORS 16u

typedef struct vf2_basic_block {
    uint32_t start;
    uint32_t end;
    uint32_t function_address;
    uint32_t successors[VF2_MAX_BLOCK_SUCCESSORS];
    uint8_t successor_count;
    bool terminal;
    bool has_indirect_flow;
} vf2_basic_block;

typedef struct vf2_function {
    uint32_t address;
    char name[64];
    uint32_t end;
    size_t first_block;
    size_t block_count;
    bool has_indirect_flow;
    bool confirmed;
    bool leaf;
    bool uses_frame_pointer;
    bool has_return_value;
    uint32_t stack_frame_size;
    uint16_t argument_register_mask;
    uint16_t return_register_mask;
    size_t resolved_indirect_count;
    size_t unresolved_indirect_count;
    size_t tail_call_count;
    size_t split_candidate_count;
    bool user_named;
} vf2_function;

typedef struct vf2_i960_analysis {
    const uint8_t *image;
    size_t image_size;
    vf2_image_class *image_map;
    vf2_basic_block *blocks;
    size_t block_count;
    size_t block_capacity;
    vf2_function *functions;
    size_t function_count;
    size_t function_capacity;
    vf2_xref *xrefs;
    size_t xref_count;
    size_t xref_capacity;
    size_t decoded_instruction_count;
    size_t invalid_instruction_count;
    vf2_constant_fact *constant_facts;
    size_t constant_fact_count;
    size_t constant_fact_capacity;
    vf2_indirect_target *indirect_targets;
    size_t indirect_target_count;
    size_t indirect_target_capacity;
    size_t resolved_indirect_count;
    size_t unresolved_indirect_count;
    vf2_function_split *function_splits;
    size_t function_split_count;
    size_t function_split_capacity;
} vf2_i960_analysis;

vf2_status vf2_i960_analysis_init(
    vf2_i960_analysis *analysis,
    const uint8_t *image,
    size_t image_size
);

void vf2_i960_analysis_destroy(vf2_i960_analysis *analysis);

vf2_status vf2_i960_analyze(
    vf2_i960_analysis *analysis,
    const uint32_t *entry_points,
    size_t entry_point_count
);

const vf2_function *vf2_i960_find_function(
    const vf2_i960_analysis *analysis,
    uint32_t address
);

vf2_status vf2_i960_analysis_record_xref(
    vf2_i960_analysis *analysis,
    uint32_t source,
    uint32_t target,
    vf2_xref_type type
);

vf2_status vf2_i960_write_analysis(
    const vf2_i960_analysis *analysis,
    const char *output_directory
);

#endif
