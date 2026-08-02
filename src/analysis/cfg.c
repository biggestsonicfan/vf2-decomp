#include "vf2/analysis/cfg.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/file.h"
#include "vf2/i960/decoder.h"
#include "vf2/analysis/pseudoc.h"

typedef struct address_queue {
    uint32_t *items;
    size_t count;
    size_t capacity;
    size_t cursor;
} address_queue;

static vf2_status reserve_array(
    void **data,
    size_t element_size,
    size_t *capacity,
    size_t required
)
{
    size_t new_capacity = 0u;
    void *new_data = NULL;

    if (required <= *capacity) {
        return VF2_OK;
    }

    new_capacity = *capacity == 0u ? 32u : *capacity;
    while (new_capacity < required) {
        if (new_capacity > (SIZE_MAX / 2u)) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        new_capacity *= 2u;
    }

    new_data = realloc(*data, element_size * new_capacity);
    if (new_data == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    *data = new_data;
    *capacity = new_capacity;
    return VF2_OK;
}

static bool queue_contains(const address_queue *queue, uint32_t address)
{
    size_t index = 0u;
    for (index = 0u; index < queue->count; ++index) {
        if (queue->items[index] == address) {
            return true;
        }
    }
    return false;
}

static vf2_status queue_push_unique(address_queue *queue, uint32_t address)
{
    vf2_status status = VF2_OK;

    if (queue_contains(queue, address)) {
        return VF2_OK;
    }

    status = reserve_array(
        (void **)&queue->items,
        sizeof(queue->items[0]),
        &queue->capacity,
        queue->count + 1u
    );
    if (status != VF2_OK) {
        return status;
    }

    queue->items[queue->count++] = address;
    return VF2_OK;
}

static bool is_valid_code_address(
    const vf2_i960_analysis *analysis,
    uint32_t address
)
{
    return (address & 3u) == 0u && (size_t)address + 4u <= analysis->image_size;
}

static bool has_function(
    const vf2_i960_analysis *analysis,
    uint32_t address
)
{
    size_t index = 0u;
    for (index = 0u; index < analysis->function_count; ++index) {
        if (analysis->functions[index].address == address) {
            return true;
        }
    }
    return false;
}

static vf2_status add_xref(
    vf2_i960_analysis *analysis,
    uint32_t source,
    uint32_t target,
    vf2_xref_type type
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < analysis->xref_count; ++index) {
        const vf2_xref *xref = &analysis->xrefs[index];
        if (xref->source == source && xref->target == target &&
            xref->type == type) {
            return VF2_OK;
        }
    }

    status = reserve_array(
        (void **)&analysis->xrefs,
        sizeof(analysis->xrefs[0]),
        &analysis->xref_capacity,
        analysis->xref_count + 1u
    );
    if (status != VF2_OK) {
        return status;
    }

    analysis->xrefs[analysis->xref_count].source = source;
    analysis->xrefs[analysis->xref_count].target = target;
    analysis->xrefs[analysis->xref_count].type = type;
    ++analysis->xref_count;
    return VF2_OK;
}

vf2_status vf2_i960_analysis_record_xref(
    vf2_i960_analysis *analysis,
    uint32_t source,
    uint32_t target,
    vf2_xref_type type
)
{
    if (analysis == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return add_xref(analysis, source, target, type);
}

static vf2_status add_block(
    vf2_i960_analysis *analysis,
    const vf2_basic_block *block
)
{
    vf2_status status = reserve_array(
        (void **)&analysis->blocks,
        sizeof(analysis->blocks[0]),
        &analysis->block_capacity,
        analysis->block_count + 1u
    );
    if (status != VF2_OK) {
        return status;
    }
    analysis->blocks[analysis->block_count++] = *block;
    return VF2_OK;
}

static vf2_status add_function(
    vf2_i960_analysis *analysis,
    uint32_t address,
    vf2_function **function_out
)
{
    vf2_function function;
    vf2_status status = VF2_OK;

    memset(&function, 0, sizeof(function));
    function.address = address;
    (void)snprintf(function.name, sizeof(function.name), "sub_%08x", (unsigned)address);
    function.end = address;
    function.first_block = analysis->block_count;
    function.confirmed = true;

    status = reserve_array(
        (void **)&analysis->functions,
        sizeof(analysis->functions[0]),
        &analysis->function_capacity,
        analysis->function_count + 1u
    );
    if (status != VF2_OK) {
        return status;
    }

    analysis->functions[analysis->function_count] = function;
    *function_out = &analysis->functions[analysis->function_count];
    ++analysis->function_count;
    return VF2_OK;
}

static bool mark_code(
    vf2_i960_analysis *analysis,
    uint32_t address,
    uint8_t size
)
{
    size_t index = 0u;
    const bool newly_discovered =
        analysis->image_map[address] != VF2_IMAGE_CODE;
    for (index = 0u; index < size; ++index) {
        if ((size_t)address + index < analysis->image_size) {
            analysis->image_map[(size_t)address + index] = VF2_IMAGE_CODE;
        }
    }
    return newly_discovered;
}

static bool memory_target(
    const vf2_i960_operand *operand,
    uint32_t *target
)
{
    const vf2_i960_memory_operand *memory = NULL;
    if (operand->kind != VF2_I960_OPERAND_MEMORY) {
        return false;
    }
    memory = &operand->value.memory;
    if (!memory->absolute && !memory->ip_relative) {
        return false;
    }
    *target = memory->resolved_address;
    return true;
}

static vf2_status record_instruction_xrefs(
    vf2_i960_analysis *analysis,
    const vf2_i960_instruction *instruction,
    address_queue *function_queue
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (instruction->has_target) {
        const vf2_xref_type type = instruction->flow == VF2_I960_FLOW_CALL
            ? VF2_XREF_CALL
            : VF2_XREF_BRANCH;
        status = add_xref(
            analysis,
            instruction->address,
            instruction->target,
            type
        );
        if (status != VF2_OK) {
            return status;
        }
        if (instruction->flow == VF2_I960_FLOW_CALL &&
            is_valid_code_address(analysis, instruction->target)) {
            status = queue_push_unique(function_queue, instruction->target);
            if (status != VF2_OK) {
                return status;
            }
        }
    }

    for (index = 0u; index < instruction->operand_count; ++index) {
        uint32_t target = 0u;
        vf2_xref_type type = VF2_XREF_READ;
        if (!memory_target(&instruction->operands[index], &target)) {
            continue;
        }
        if (instruction->flow != VF2_I960_FLOW_NONE) {
            continue;
        }
        if (strcmp(instruction->mnemonic, "lda") == 0) {
            type = VF2_XREF_ADDRESS;
        } else if (instruction->operands[index].is_destination) {
            type = VF2_XREF_WRITE;
        }
        status = add_xref(
            analysis,
            instruction->address,
            target,
            type
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    return VF2_OK;
}

static bool address_is_leader(
    const uint8_t *leaders,
    size_t word_count,
    uint32_t address
)
{
    const size_t word = (size_t)address / 4u;
    return word < word_count && leaders[word] != 0u;
}

static bool address_is_reachable(
    const uint8_t *reachable,
    size_t word_count,
    uint32_t address
)
{
    const size_t word = (size_t)address / 4u;
    return word < word_count && reachable[word] == 1u;
}

static vf2_status discover_reachable_instructions(
    vf2_i960_analysis *analysis,
    vf2_function *function,
    address_queue *function_queue,
    uint8_t *reachable,
    uint8_t *leaders,
    size_t word_count
)
{
    address_queue path_queue;
    vf2_status status = VF2_OK;

    memset(&path_queue, 0, sizeof(path_queue));
    leaders[(size_t)function->address / 4u] = 1u;
    status = queue_push_unique(&path_queue, function->address);

    while (status == VF2_OK && path_queue.cursor < path_queue.count) {
        uint32_t address = path_queue.items[path_queue.cursor++];

        while (is_valid_code_address(analysis, address)) {
            const size_t word_index = (size_t)address / 4u;
            vf2_i960_instruction instruction;
            vf2_status decode_status = VF2_OK;
            uint32_t next = 0u;

            if (word_index >= word_count || reachable[word_index] != 0u) {
                break;
            }

            decode_status = vf2_i960_decode(
                analysis->image,
                analysis->image_size,
                address,
                &instruction
            );
            if (decode_status != VF2_OK) {
                reachable[word_index] = 3u;
                ++analysis->invalid_instruction_count;
                break;
            }

            reachable[word_index] = 1u;
            if (instruction.size == 8u && word_index + 1u < word_count) {
                reachable[word_index + 1u] = 2u;
            }
            if (mark_code(analysis, address, instruction.size)) {
                ++analysis->decoded_instruction_count;
            }

            status = record_instruction_xrefs(
                analysis,
                &instruction,
                function_queue
            );
            if (status != VF2_OK) {
                break;
            }

            next = address + instruction.size;
            if (instruction.flow == VF2_I960_FLOW_BRANCH) {
                if (instruction.has_target &&
                    is_valid_code_address(analysis, instruction.target)) {
                    leaders[(size_t)instruction.target / 4u] = 1u;
                    status = queue_push_unique(&path_queue, instruction.target);
                    if (status != VF2_OK) {
                        break;
                    }
                }
                if (instruction.has_fallthrough &&
                    is_valid_code_address(analysis, next)) {
                    leaders[(size_t)next / 4u] = 1u;
                    status = queue_push_unique(&path_queue, next);
                }
                if (instruction.indirect) {
                    function->has_indirect_flow = true;
                }
                break;
            }

            if (instruction.flow == VF2_I960_FLOW_RETURN ||
                instruction.flow == VF2_I960_FLOW_FAULT ||
                !instruction.has_fallthrough) {
                break;
            }

            if (instruction.flow == VF2_I960_FLOW_CALL &&
                instruction.indirect) {
                function->has_indirect_flow = true;
            }

            address = next;
        }
    }

    free(path_queue.items);
    return status;
}

static vf2_status build_function_blocks(
    vf2_i960_analysis *analysis,
    vf2_function *function,
    const uint8_t *reachable,
    const uint8_t *leaders,
    size_t word_count
)
{
    size_t word_index = 0u;
    vf2_status status = VF2_OK;

    for (word_index = 0u; word_index < word_count; ++word_index) {
        uint32_t address = 0u;
        vf2_basic_block block;

        if (leaders[word_index] == 0u || reachable[word_index] != 1u) {
            continue;
        }

        address = (uint32_t)(word_index * 4u);
        memset(&block, 0, sizeof(block));
        block.start = address;
        block.end = address;
        block.function_address = function->address;

        while (address_is_reachable(reachable, word_count, address)) {
            vf2_i960_instruction instruction;
            uint32_t next = 0u;

            status = vf2_i960_decode(
                analysis->image,
                analysis->image_size,
                address,
                &instruction
            );
            if (status != VF2_OK) {
                block.terminal = true;
                status = VF2_OK;
                break;
            }

            next = address + instruction.size;
            block.end = next;
            if (block.end > function->end) {
                function->end = block.end;
            }

            if (instruction.flow == VF2_I960_FLOW_BRANCH) {
                if (instruction.has_target &&
                    address_is_reachable(
                        reachable,
                        word_count,
                        instruction.target
                    )) {
                    block.successors[block.successor_count++] =
                        instruction.target;
                }
                if (instruction.has_fallthrough &&
                    block.successor_count < VF2_MAX_BLOCK_SUCCESSORS &&
                    address_is_reachable(reachable, word_count, next)) {
                    block.successors[block.successor_count++] = next;
                }
                block.has_indirect_flow = instruction.indirect;
                block.terminal = !instruction.has_fallthrough;
                break;
            }

            if (instruction.flow == VF2_I960_FLOW_RETURN ||
                instruction.flow == VF2_I960_FLOW_FAULT ||
                !instruction.has_fallthrough) {
                block.terminal = true;
                break;
            }

            if (address_is_leader(leaders, word_count, next)) {
                if (address_is_reachable(reachable, word_count, next)) {
                    block.successors[0] = next;
                    block.successor_count = 1u;
                }
                break;
            }

            address = next;
        }

        status = add_block(analysis, &block);
        if (status != VF2_OK) {
            return status;
        }
    }

    return status;
}

static vf2_status analyze_function(
    vf2_i960_analysis *analysis,
    vf2_function *function,
    address_queue *function_queue
)
{
    const size_t word_count = (analysis->image_size + 3u) / 4u;
    uint8_t *reachable = NULL;
    uint8_t *leaders = NULL;
    vf2_status status = VF2_OK;

    reachable = (uint8_t *)calloc(word_count, sizeof(reachable[0]));
    leaders = (uint8_t *)calloc(word_count, sizeof(leaders[0]));
    if (reachable == NULL || leaders == NULL) {
        free(reachable);
        free(leaders);
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    status = discover_reachable_instructions(
        analysis,
        function,
        function_queue,
        reachable,
        leaders,
        word_count
    );
    if (status == VF2_OK) {
        status = build_function_blocks(
            analysis,
            function,
            reachable,
            leaders,
            word_count
        );
    }

    function->block_count = analysis->block_count - function->first_block;
    free(reachable);
    free(leaders);
    return status;
}

static bool is_printable_string_byte(uint8_t value)
{
    return value == (uint8_t)'\t' ||
           (value >= 0x20u && value <= 0x7eu);
}

static vf2_status classify_strings(vf2_i960_analysis *analysis)
{
    size_t offset = 0u;
    vf2_status status = VF2_OK;

    while (offset < analysis->image_size) {
        size_t end = offset;
        size_t xref_index = 0u;

        if (analysis->image_map[offset] != VF2_IMAGE_UNKNOWN ||
            !is_printable_string_byte(analysis->image[offset])) {
            ++offset;
            continue;
        }

        while (end < analysis->image_size &&
               analysis->image_map[end] == VF2_IMAGE_UNKNOWN &&
               is_printable_string_byte(analysis->image[end])) {
            ++end;
        }

        if (end - offset >= 5u &&
            end < analysis->image_size && analysis->image[end] == 0u) {
            size_t index = 0u;
            for (index = offset; index < end; ++index) {
                analysis->image_map[index] = VF2_IMAGE_STRING;
            }
            for (xref_index = 0u;
                 xref_index < analysis->xref_count;
                 ++xref_index) {
                const vf2_xref xref = analysis->xrefs[xref_index];
                if (xref.target >= offset && xref.target < end) {
                    status = add_xref(
                        analysis,
                        xref.source,
                        (uint32_t)offset,
                        VF2_XREF_STRING
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                }
            }
        }
        offset = end == offset ? offset + 1u : end;
    }
    return status;
}

static void classify_padding(vf2_i960_analysis *analysis)
{
    size_t offset = 0u;
    while (offset < analysis->image_size) {
        const uint8_t value = analysis->image[offset];
        size_t end = offset;
        size_t index = 0u;

        if (analysis->image_map[offset] != VF2_IMAGE_UNKNOWN ||
            (value != 0u && value != 0xffu)) {
            ++offset;
            continue;
        }

        while (end < analysis->image_size &&
               analysis->image_map[end] == VF2_IMAGE_UNKNOWN &&
               analysis->image[end] == value) {
            ++end;
        }

        if (end - offset >= 16u) {
            for (index = offset; index < end; ++index) {
                analysis->image_map[index] = VF2_IMAGE_PADDING;
            }
        }
        offset = end;
    }
}

vf2_status vf2_i960_analysis_init(
    vf2_i960_analysis *analysis,
    const uint8_t *image,
    size_t image_size
)
{
    if (analysis == NULL || image == NULL || image_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(analysis, 0, sizeof(*analysis));
    analysis->image_map = (vf2_image_class *)calloc(
        image_size,
        sizeof(analysis->image_map[0])
    );
    if (analysis->image_map == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    analysis->image = image;
    analysis->image_size = image_size;
    return VF2_OK;
}

void vf2_i960_analysis_destroy(vf2_i960_analysis *analysis)
{
    if (analysis == NULL) {
        return;
    }
    free(analysis->image_map);
    free(analysis->blocks);
    free(analysis->functions);
    free(analysis->xrefs);
    free(analysis->constant_facts);
    free(analysis->indirect_targets);
    free(analysis->function_splits);
    memset(analysis, 0, sizeof(*analysis));
}

vf2_status vf2_i960_analyze(
    vf2_i960_analysis *analysis,
    const uint32_t *entry_points,
    size_t entry_point_count
)
{
    address_queue function_queue;
    vf2_status status = VF2_OK;
    size_t index = 0u;

    if (analysis == NULL || entry_points == NULL || entry_point_count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&function_queue, 0, sizeof(function_queue));
    for (index = 0u; index < entry_point_count; ++index) {
        if (is_valid_code_address(analysis, entry_points[index])) {
            status = queue_push_unique(&function_queue, entry_points[index]);
            if (status != VF2_OK) {
                break;
            }
        }
    }

    while (status == VF2_OK &&
           function_queue.cursor < function_queue.count) {
        const uint32_t address = function_queue.items[function_queue.cursor++];
        vf2_function *function = NULL;

        if (has_function(analysis, address)) {
            continue;
        }

        status = add_function(analysis, address, &function);
        if (status == VF2_OK) {
            status = analyze_function(analysis, function, &function_queue);
        }
    }

    if (status == VF2_OK) {
        status = vf2_i960_run_semantic_analysis(analysis);
    }
    if (status == VF2_OK) {
        status = vf2_i960_detect_function_boundaries(analysis);
    }
    if (status == VF2_OK) {
        status = classify_strings(analysis);
    }
    if (status == VF2_OK) {
        classify_padding(analysis);
    }

    free(function_queue.items);
    return status;
}

const vf2_function *vf2_i960_find_function(
    const vf2_i960_analysis *analysis,
    uint32_t address
)
{
    size_t index = 0u;
    if (analysis == NULL) {
        return NULL;
    }
    for (index = 0u; index < analysis->function_count; ++index) {
        if (analysis->functions[index].address == address) {
            return &analysis->functions[index];
        }
    }
    return NULL;
}

static vf2_status open_output_file(
    const char *directory,
    const char *filename,
    FILE **file_out
)
{
    char path[4096];
    vf2_status status = vf2_join_path(
        path,
        sizeof(path),
        directory,
        filename
    );
    if (status != VF2_OK) {
        return status;
    }
    *file_out = fopen(path, "wb");
    return *file_out == NULL ? VF2_ERROR_IO : VF2_OK;
}

static void csv_string(FILE *file, const uint8_t *data, size_t size)
{
    size_t index = 0u;
    fputc('"', file);
    for (index = 0u; index < size; ++index) {
        if (data[index] == (uint8_t)'"') {
            fputc('"', file);
        }
        if (data[index] == (uint8_t)'\t') {
            fputs("\\t", file);
        } else {
            fputc((int)data[index], file);
        }
    }
    fputc('"', file);
}

static vf2_status write_functions(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "functions.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs(
        "address,end,name,status,blocks,indirect-flow,leaf,frame-size,"
        "argument-mask,return-mask,resolved-indirect,unresolved-indirect,"
        "tail-calls,split-candidates,user-named\n",
        file
    );
    for (index = 0u; index < analysis->function_count; ++index) {
        const vf2_function *function = &analysis->functions[index];
        fprintf(
            file,
            "0x%08x,0x%08x,%s,candidate,%zu,%s,%s,0x%x,0x%04x,0x%04x,%zu,%zu,%zu,%zu,%s\n",
            (unsigned)function->address,
            (unsigned)function->end,
            function->name,
            function->block_count,
            function->has_indirect_flow ? "yes" : "no",
            function->leaf ? "yes" : "no",
            (unsigned)function->stack_frame_size,
            (unsigned)function->argument_register_mask,
            (unsigned)function->return_register_mask,
            function->resolved_indirect_count,
            function->unresolved_indirect_count,
            function->tail_call_count,
            function->split_candidate_count,
            function->user_named ? "yes" : "no"
        );
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_xrefs(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "xrefs.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("source,target,type\n", file);
    for (index = 0u; index < analysis->xref_count; ++index) {
        const vf2_xref *xref = &analysis->xrefs[index];
        fprintf(
            file,
            "0x%08x,0x%08x,%s\n",
            (unsigned)xref->source,
            (unsigned)xref->target,
            vf2_xref_type_name(xref->type)
        );
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_image_map(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t start = 0u;
    vf2_status status = open_output_file(directory, "image-map.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("start,end,size,class\n", file);
    while (start < analysis->image_size) {
        const vf2_image_class classification = analysis->image_map[start];
        size_t end = start + 1u;
        while (end < analysis->image_size &&
               analysis->image_map[end] == classification) {
            ++end;
        }
        fprintf(
            file,
            "0x%08zx,0x%08zx,0x%zx,%s\n",
            start,
            end,
            end - start,
            vf2_image_class_name(classification)
        );
        start = end;
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_disassembly(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t address = 0u;
    vf2_status status = open_output_file(directory, "i960.asm", &file);
    if (status != VF2_OK) {
        return status;
    }
    while (address + 4u <= analysis->image_size) {
        if (analysis->image_map[address] == VF2_IMAGE_CODE) {
            vf2_i960_instruction instruction;
            char text[256];
            if (vf2_i960_decode(
                    analysis->image,
                    analysis->image_size,
                    (uint32_t)address,
                    &instruction
                ) == VF2_OK &&
                vf2_i960_format_instruction(
                    &instruction,
                    text,
                    sizeof(text)
                ) == VF2_OK) {
                fprintf(
                    file,
                    "%08zx  %08x  %s\n",
                    address,
                    (unsigned)instruction.words[0],
                    text
                );
                address += instruction.size;
                continue;
            }
        }
        address += 4u;
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_strings(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t offset = 0u;
    vf2_status status = open_output_file(directory, "strings.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("address,length,xrefs,text\n", file);
    while (offset < analysis->image_size) {
        if (analysis->image_map[offset] == VF2_IMAGE_STRING &&
            (offset == 0u || analysis->image_map[offset - 1u] != VF2_IMAGE_STRING)) {
            size_t end = offset;
            size_t xrefs = 0u;
            size_t index = 0u;
            while (end < analysis->image_size &&
                   analysis->image_map[end] == VF2_IMAGE_STRING &&
                   analysis->image[end] != 0u) {
                ++end;
            }
            for (index = 0u; index < analysis->xref_count; ++index) {
                if (analysis->xrefs[index].type == VF2_XREF_STRING &&
                    analysis->xrefs[index].target == offset) {
                    ++xrefs;
                }
            }
            fprintf(file, "0x%08zx,%zu,%zu,", offset, end - offset, xrefs);
            csv_string(file, analysis->image + offset, end - offset);
            fputc('\n', file);
            offset = end + 1u;
        } else {
            ++offset;
        }
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static const vf2_function *find_containing_function(
    const vf2_i960_analysis *analysis,
    uint32_t address
)
{
    size_t function_index = 0u;
    for (function_index = 0u;
         function_index < analysis->function_count;
         ++function_index) {
        const vf2_function *function = &analysis->functions[function_index];
        size_t block_index = 0u;
        for (block_index = function->first_block;
             block_index < function->first_block + function->block_count;
             ++block_index) {
            const vf2_basic_block *block = &analysis->blocks[block_index];
            if (address >= block->start && address < block->end) {
                return function;
            }
        }
    }
    return NULL;
}

static vf2_status write_callgraph(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "callgraph.dot", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("digraph callgraph {\n  node [shape=box,fontname=monospace];\n", file);
    for (index = 0u; index < analysis->function_count; ++index) {
        fprintf(
            file,
            "  f_%08x [label=\"%s\"];\n",
            (unsigned)analysis->functions[index].address,
            analysis->functions[index].name
        );
    }
    for (index = 0u; index < analysis->xref_count; ++index) {
        const vf2_xref *xref = &analysis->xrefs[index];
        const vf2_function *source_function = NULL;
        if (xref->type != VF2_XREF_CALL ||
            !has_function(analysis, xref->target)) {
            continue;
        }
        source_function = find_containing_function(analysis, xref->source);
        if (source_function != NULL) {
            fprintf(
                file,
                "  f_%08x -> f_%08x;\n",
                (unsigned)source_function->address,
                (unsigned)xref->target
            );
        }
    }
    fputs("}\n", file);
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_cfgs(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    char cfg_directory[4096];
    size_t function_index = 0u;
    vf2_status status = vf2_join_path(
        cfg_directory,
        sizeof(cfg_directory),
        directory,
        "cfg"
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_make_directory(cfg_directory);
    if (status != VF2_OK) {
        return status;
    }

    for (function_index = 0u;
         function_index < analysis->function_count;
         ++function_index) {
        const vf2_function *function = &analysis->functions[function_index];
        char filename[64];
        char path[4096];
        FILE *file = NULL;
        size_t index = 0u;

        (void)snprintf(
            filename,
            sizeof(filename),
            "%08x.dot",
            (unsigned)function->address
        );
        status = vf2_join_path(path, sizeof(path), cfg_directory, filename);
        if (status != VF2_OK) {
            return status;
        }
        file = fopen(path, "wb");
        if (file == NULL) {
            return VF2_ERROR_IO;
        }
        fprintf(
            file,
            "digraph cfg_%08x {\n  node [shape=box,fontname=monospace];\n",
            (unsigned)function->address
        );
        for (index = function->first_block;
             index < function->first_block + function->block_count;
             ++index) {
            const vf2_basic_block *block = &analysis->blocks[index];
            size_t successor = 0u;
            fprintf(
                file,
                "  b_%08x [label=\"%08x-%08x%s\"];\n",
                (unsigned)block->start,
                (unsigned)block->start,
                (unsigned)block->end,
                block->has_indirect_flow ? "\\nindirect" : ""
            );
            for (successor = 0u;
                 successor < block->successor_count;
                 ++successor) {
                fprintf(
                    file,
                    "  b_%08x -> b_%08x;\n",
                    (unsigned)block->start,
                    (unsigned)block->successors[successor]
                );
            }
        }
        fputs("}\n", file);
        if (fclose(file) != 0) {
            return VF2_ERROR_IO;
        }
    }
    return VF2_OK;
}

static vf2_status write_constant_facts(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "values.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("address,register,kind,value,table-base,scale,argument-register\n", file);
    for (index = 0u; index < analysis->constant_fact_count; ++index) {
        const vf2_constant_fact *fact = &analysis->constant_facts[index];
        fprintf(
            file,
            "0x%08x,%s,%s,0x%llx,0x%08x,%u,%s\n",
            (unsigned)fact->address,
            vf2_i960_register_name(fact->reg),
            vf2_value_kind_name(fact->value.kind),
            (unsigned long long)fact->value.value,
            (unsigned)fact->value.table_base,
            (unsigned)fact->value.table_scale,
            fact->value.kind == VF2_VALUE_ARGUMENT
                ? vf2_i960_register_name(fact->value.argument_register)
                : ""
        );
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_indirect_targets(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "indirect-targets.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("source,target,kind,table-base,confidence\n", file);
    for (index = 0u; index < analysis->indirect_target_count; ++index) {
        const vf2_indirect_target *target = &analysis->indirect_targets[index];
        fprintf(
            file,
            "0x%08x,0x%08x,%s,0x%08x,%u\n",
            (unsigned)target->source,
            (unsigned)target->target,
            vf2_indirect_target_kind_name(target->kind),
            (unsigned)target->table_base,
            (unsigned)target->confidence
        );
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_stack_frames(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "stack-frames.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("function,frame-size,uses-fp,leaf,arguments,returns\n", file);
    for (index = 0u; index < analysis->function_count; ++index) {
        const vf2_function *function = &analysis->functions[index];
        fprintf(
            file,
            "0x%08x,0x%x,%s,%s,0x%04x,0x%04x\n",
            (unsigned)function->address,
            (unsigned)function->stack_frame_size,
            function->uses_frame_pointer ? "yes" : "no",
            function->leaf ? "yes" : "no",
            (unsigned)function->argument_register_mask,
            (unsigned)function->return_register_mask
        );
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_function_splits(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "function-splits.csv", &file);
    if (status != VF2_OK) {
        return status;
    }
    fputs("source-function,target-function,instruction,reason\n", file);
    for (index = 0u; index < analysis->function_split_count; ++index) {
        const vf2_function_split *split = &analysis->function_splits[index];
        fprintf(
            file,
            "0x%08x,0x%08x,0x%08x,%s\n",
            (unsigned)split->source_function,
            (unsigned)split->target_function,
            (unsigned)split->instruction_address,
            vf2_split_reason_name(split->reason)
        );
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status write_report(
    const vf2_i960_analysis *analysis,
    const char *directory
)
{
    FILE *file = NULL;
    size_t counts[6] = {0u};
    size_t index = 0u;
    vf2_status status = open_output_file(directory, "report.json", &file);
    if (status != VF2_OK) {
        return status;
    }
    for (index = 0u; index < analysis->image_size; ++index) {
        const unsigned classification = (unsigned)analysis->image_map[index];
        if (classification < 6u) {
            ++counts[classification];
        }
    }
    fprintf(
        file,
        "{\n"
        "  \"rom_size\": %zu,\n"
        "  \"decoded_instructions\": %zu,\n"
        "  \"invalid_instructions\": %zu,\n"
        "  \"discovered_functions\": %zu,\n"
        "  \"basic_blocks\": %zu,\n"
        "  \"xrefs\": %zu,\n"
        "  \"constant_facts\": %zu,\n"
        "  \"indirect_targets\": %zu,\n"
        "  \"resolved_indirect\": %zu,\n"
        "  \"unresolved_indirect\": %zu,\n"
        "  \"function_split_candidates\": %zu,\n"
        "  \"code_bytes\": %zu,\n"
        "  \"string_bytes\": %zu,\n"
        "  \"padding_bytes\": %zu,\n"
        "  \"unknown_bytes\": %zu\n"
        "}\n",
        analysis->image_size,
        analysis->decoded_instruction_count,
        analysis->invalid_instruction_count,
        analysis->function_count,
        analysis->block_count,
        analysis->xref_count,
        analysis->constant_fact_count,
        analysis->indirect_target_count,
        analysis->resolved_indirect_count,
        analysis->unresolved_indirect_count,
        analysis->function_split_count,
        counts[VF2_IMAGE_CODE],
        counts[VF2_IMAGE_STRING],
        counts[VF2_IMAGE_PADDING],
        counts[VF2_IMAGE_UNKNOWN]
    );
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

vf2_status vf2_i960_write_analysis(
    const vf2_i960_analysis *analysis,
    const char *output_directory
)
{
    vf2_status status = VF2_OK;
    if (analysis == NULL || output_directory == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_make_directories(output_directory);
    if (status == VF2_OK) {
        status = write_report(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_functions(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_xrefs(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_image_map(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_disassembly(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_strings(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_constant_facts(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_indirect_targets(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_stack_frames(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_function_splits(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_callgraph(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = write_cfgs(analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = vf2_i960_write_all_pseudoc(analysis, output_directory);
    }
    return status;
}
