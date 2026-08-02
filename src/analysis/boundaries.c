#include "vf2/analysis/boundaries.h"

#include <stdbool.h>
#include <stdlib.h>

#include "vf2/analysis/cfg.h"
#include "vf2/i960/decoder.h"

static vf2_status reserve_splits(vf2_i960_analysis *analysis, size_t required)
{
    size_t capacity = 0u;
    void *data = NULL;
    if (required <= analysis->function_split_capacity) {
        return VF2_OK;
    }
    capacity = analysis->function_split_capacity == 0u
        ? 32u
        : analysis->function_split_capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        capacity *= 2u;
    }
    data = realloc(analysis->function_splits, capacity * sizeof(analysis->function_splits[0]));
    if (data == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    analysis->function_splits = (vf2_function_split *)data;
    analysis->function_split_capacity = capacity;
    return VF2_OK;
}

static vf2_function *find_function_mutable(vf2_i960_analysis *analysis, uint32_t address)
{
    size_t index = 0u;
    for (index = 0u; index < analysis->function_count; ++index) {
        if (analysis->functions[index].address == address) {
            return &analysis->functions[index];
        }
    }
    return NULL;
}

static bool function_contains_address(
    const vf2_i960_analysis *analysis,
    const vf2_function *function,
    uint32_t address
)
{
    size_t block_index = 0u;
    for (block_index = function->first_block;
         block_index < function->first_block + function->block_count;
         ++block_index) {
        const vf2_basic_block *block = &analysis->blocks[block_index];
        if (address >= block->start && address < block->end) {
            return true;
        }
    }
    return false;
}

static vf2_status add_split(
    vf2_i960_analysis *analysis,
    uint32_t source_function,
    uint32_t target_function,
    uint32_t instruction_address,
    vf2_split_reason reason
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;
    for (index = 0u; index < analysis->function_split_count; ++index) {
        const vf2_function_split *split = &analysis->function_splits[index];
        if (split->source_function == source_function &&
            split->target_function == target_function &&
            split->instruction_address == instruction_address &&
            split->reason == reason) {
            return VF2_OK;
        }
    }
    status = reserve_splits(analysis, analysis->function_split_count + 1u);
    if (status != VF2_OK) {
        return status;
    }
    analysis->function_splits[analysis->function_split_count].source_function = source_function;
    analysis->function_splits[analysis->function_split_count].target_function = target_function;
    analysis->function_splits[analysis->function_split_count].instruction_address = instruction_address;
    analysis->function_splits[analysis->function_split_count].reason = reason;
    ++analysis->function_split_count;
    return VF2_OK;
}

static bool decode_last_instruction(
    const vf2_i960_analysis *analysis,
    const vf2_basic_block *block,
    vf2_i960_instruction *instruction
)
{
    uint32_t address = block->start;
    bool decoded = false;
    while (address < block->end) {
        vf2_i960_instruction current;
        if (vf2_i960_decode(
                analysis->image,
                analysis->image_size,
                address,
                &current
            ) != VF2_OK) {
            break;
        }
        *instruction = current;
        decoded = true;
        address += current.size;
    }
    return decoded;
}

const char *vf2_split_reason_name(vf2_split_reason reason)
{
    switch (reason) {
    case VF2_SPLIT_TAIL_BRANCH: return "tail-branch";
    case VF2_SPLIT_OVERLAPPING_ENTRY:
    default: return "overlapping-entry";
    }
}

vf2_status vf2_i960_detect_function_boundaries(vf2_i960_analysis *analysis)
{
    size_t source_index = 0u;
    vf2_status status = VF2_OK;
    if (analysis == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    free(analysis->function_splits);
    analysis->function_splits = NULL;
    analysis->function_split_count = 0u;
    analysis->function_split_capacity = 0u;
    for (source_index = 0u; source_index < analysis->function_count; ++source_index) {
        analysis->functions[source_index].tail_call_count = 0u;
        analysis->functions[source_index].split_candidate_count = 0u;
    }

    for (source_index = 0u; source_index < analysis->function_count; ++source_index) {
        vf2_function *source = &analysis->functions[source_index];
        size_t target_index = 0u;
        size_t block_index = 0u;

        for (target_index = 0u; target_index < analysis->function_count; ++target_index) {
            const vf2_function *target = &analysis->functions[target_index];
            if (target->address == source->address) {
                continue;
            }
            if (function_contains_address(analysis, source, target->address)) {
                status = add_split(
                    analysis,
                    source->address,
                    target->address,
                    target->address,
                    VF2_SPLIT_OVERLAPPING_ENTRY
                );
                if (status != VF2_OK) {
                    return status;
                }
                ++source->split_candidate_count;
            }
        }

        for (block_index = source->first_block;
             block_index < source->first_block + source->block_count;
             ++block_index) {
            vf2_i960_instruction last;
            const vf2_basic_block *block = &analysis->blocks[block_index];
            vf2_function *target = NULL;
            if (!decode_last_instruction(analysis, block, &last) ||
                last.flow != VF2_I960_FLOW_BRANCH ||
                last.has_fallthrough || !last.has_target) {
                continue;
            }
            target = find_function_mutable(analysis, last.target);
            if (target == NULL || target->address == source->address) {
                continue;
            }
            status = add_split(
                analysis,
                source->address,
                target->address,
                last.address,
                VF2_SPLIT_TAIL_BRANCH
            );
            if (status != VF2_OK) {
                return status;
            }
            status = vf2_i960_analysis_record_xref(
                analysis,
                last.address,
                target->address,
                VF2_XREF_CALL
            );
            if (status != VF2_OK) {
                return status;
            }
            ++source->tail_call_count;
            ++source->split_candidate_count;
        }
    }
    return VF2_OK;
}
