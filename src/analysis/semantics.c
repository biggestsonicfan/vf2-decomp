#include "vf2/analysis/semantics.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/cfg.h"
#include "vf2/i960/decoder.h"

#define VF2_I960_REGISTER_COUNT 32u
#define VF2_I960_G0 16u
#define VF2_I960_G7 23u
#define VF2_I960_FP 31u
#define VF2_I960_SP 1u
#define VF2_MAX_TABLE_ENTRIES 128u

typedef struct register_state {
    vf2_abstract_value values[VF2_I960_REGISTER_COUNT];
} register_state;

static vf2_abstract_value unknown_value(void)
{
    vf2_abstract_value value;
    memset(&value, 0, sizeof(value));
    value.kind = VF2_VALUE_UNKNOWN;
    return value;
}

static vf2_abstract_value constant_value(int64_t number)
{
    vf2_abstract_value value = unknown_value();
    value.kind = VF2_VALUE_CONSTANT;
    value.value = number;
    return value;
}

static vf2_abstract_value stack_value(int64_t offset)
{
    vf2_abstract_value value = unknown_value();
    value.kind = VF2_VALUE_STACK_RELATIVE;
    value.value = offset;
    return value;
}

static vf2_abstract_value argument_value(uint8_t reg)
{
    vf2_abstract_value value = unknown_value();
    value.kind = VF2_VALUE_ARGUMENT;
    value.argument_register = reg;
    return value;
}

static vf2_abstract_value table_value(uint32_t base, uint8_t scale)
{
    vf2_abstract_value value = unknown_value();
    value.kind = VF2_VALUE_TABLE_LOOKUP;
    value.table_base = base;
    value.table_scale = scale;
    return value;
}

static bool values_equal(
    const vf2_abstract_value *left,
    const vf2_abstract_value *right
)
{
    return left->kind == right->kind &&
           left->value == right->value &&
           left->table_base == right->table_base &&
           left->table_scale == right->table_scale &&
           left->argument_register == right->argument_register;
}

static bool merge_state(register_state *destination, const register_state *source)
{
    size_t index = 0u;
    bool changed = false;
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (!values_equal(&destination->values[index], &source->values[index]) &&
            destination->values[index].kind != VF2_VALUE_UNKNOWN) {
            destination->values[index] = unknown_value();
            changed = true;
        }
    }
    return changed;
}

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
    new_capacity = *capacity == 0u ? 64u : *capacity;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2u) {
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

static vf2_status add_constant_fact(
    vf2_i960_analysis *analysis,
    uint32_t address,
    uint8_t reg,
    vf2_abstract_value value
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;
    if (value.kind == VF2_VALUE_UNKNOWN) {
        return VF2_OK;
    }
    for (index = 0u; index < analysis->constant_fact_count; ++index) {
        const vf2_constant_fact *fact = &analysis->constant_facts[index];
        if (fact->address == address && fact->reg == reg &&
            values_equal(&fact->value, &value)) {
            return VF2_OK;
        }
    }
    status = reserve_array(
        (void **)&analysis->constant_facts,
        sizeof(analysis->constant_facts[0]),
        &analysis->constant_fact_capacity,
        analysis->constant_fact_count + 1u
    );
    if (status != VF2_OK) {
        return status;
    }
    analysis->constant_facts[analysis->constant_fact_count].address = address;
    analysis->constant_facts[analysis->constant_fact_count].reg = reg;
    analysis->constant_facts[analysis->constant_fact_count].value = value;
    ++analysis->constant_fact_count;
    return VF2_OK;
}

static vf2_status add_indirect_target(
    vf2_i960_analysis *analysis,
    uint32_t source,
    uint32_t target,
    uint32_t table_base,
    vf2_indirect_target_kind kind,
    uint8_t confidence
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;
    for (index = 0u; index < analysis->indirect_target_count; ++index) {
        const vf2_indirect_target *item = &analysis->indirect_targets[index];
        if (item->source == source && item->target == target &&
            item->kind == kind) {
            return VF2_OK;
        }
    }
    status = reserve_array(
        (void **)&analysis->indirect_targets,
        sizeof(analysis->indirect_targets[0]),
        &analysis->indirect_target_capacity,
        analysis->indirect_target_count + 1u
    );
    if (status != VF2_OK) {
        return status;
    }
    analysis->indirect_targets[analysis->indirect_target_count].source = source;
    analysis->indirect_targets[analysis->indirect_target_count].target = target;
    analysis->indirect_targets[analysis->indirect_target_count].table_base = table_base;
    analysis->indirect_targets[analysis->indirect_target_count].kind = kind;
    analysis->indirect_targets[analysis->indirect_target_count].confidence = confidence;
    ++analysis->indirect_target_count;
    return VF2_OK;
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static int32_t read_le16_signed(const uint8_t *data)
{
    const uint16_t value = (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
    return (int16_t)value;
}

static vf2_abstract_value evaluate_operand(
    const vf2_i960_operand *operand,
    const register_state *state
)
{
    if (operand->kind == VF2_I960_OPERAND_REGISTER ||
        operand->kind == VF2_I960_OPERAND_FP_REGISTER ||
        operand->kind == VF2_I960_OPERAND_SPECIAL_REGISTER) {
        return state->values[operand->value.reg];
    }
    if (operand->kind == VF2_I960_OPERAND_LITERAL) {
        return constant_value(operand->value.literal);
    }
    if (operand->kind == VF2_I960_OPERAND_ADDRESS) {
        return constant_value(operand->value.address);
    }
    return unknown_value();
}

static vf2_abstract_value evaluate_memory_address(
    const vf2_i960_memory_operand *memory,
    const register_state *state
)
{
    vf2_abstract_value result = constant_value(memory->displacement);
    bool has_component = memory->absolute || memory->ip_relative ||
                         memory->displacement != 0;

    if (memory->absolute || memory->ip_relative) {
        result = constant_value(memory->resolved_address);
    }

    if (memory->has_base) {
        const vf2_abstract_value base = state->values[memory->base];
        if (base.kind == VF2_VALUE_TABLE_LOOKUP && !memory->has_index &&
            memory->displacement == 0) {
            return base;
        }
        if (base.kind == VF2_VALUE_CONSTANT) {
            result = constant_value(base.value + memory->displacement);
            has_component = true;
        } else if (base.kind == VF2_VALUE_STACK_RELATIVE) {
            result = stack_value(base.value + memory->displacement);
            has_component = true;
        } else {
            return unknown_value();
        }
    }

    if (memory->has_index) {
        const vf2_abstract_value index = state->values[memory->index];
        if (index.kind == VF2_VALUE_CONSTANT) {
            const int64_t scaled = index.value << memory->scale;
            if (result.kind == VF2_VALUE_CONSTANT) {
                result.value += scaled;
            } else if (result.kind == VF2_VALUE_STACK_RELATIVE) {
                result.value += scaled;
            } else {
                return unknown_value();
            }
        } else {
            return unknown_value();
        }
    }

    return has_component ? result : unknown_value();
}

static bool indexed_table_base(
    const vf2_i960_memory_operand *memory,
    const register_state *state,
    uint32_t *base_out
)
{
    int64_t base = memory->displacement;
    if (!memory->has_index || memory->scale != 2u) {
        return false;
    }
    if (memory->absolute || memory->ip_relative) {
        base = memory->resolved_address;
    }
    if (memory->has_base) {
        const vf2_abstract_value base_value = state->values[memory->base];
        if (base_value.kind != VF2_VALUE_CONSTANT) {
            return false;
        }
        base += base_value.value;
    }
    if (base < 0 || base > UINT32_MAX) {
        return false;
    }
    *base_out = (uint32_t)base;
    return true;
}

static vf2_abstract_value load_value(
    const vf2_i960_analysis *analysis,
    const vf2_i960_instruction *instruction,
    const register_state *state
)
{
    const vf2_i960_memory_operand *memory = &instruction->operands[0].value.memory;
    vf2_abstract_value address = evaluate_memory_address(memory, state);
    uint32_t table_base = 0u;
    if (memory->has_index &&
        state->values[memory->index].kind != VF2_VALUE_CONSTANT &&
        indexed_table_base(memory, state, &table_base) &&
        table_base + 4u <= analysis->image_size) {
        return table_value(table_base, memory->scale);
    }
    if (address.kind != VF2_VALUE_CONSTANT || address.value < 0 ||
        (uint64_t)address.value >= analysis->image_size) {
        return unknown_value();
    }
    if (strcmp(instruction->mnemonic, "ldob") == 0) {
        return constant_value(analysis->image[(size_t)address.value]);
    }
    if (strcmp(instruction->mnemonic, "ldib") == 0) {
        return constant_value((int8_t)analysis->image[(size_t)address.value]);
    }
    if (strcmp(instruction->mnemonic, "ldos") == 0) {
        if ((size_t)address.value + 2u > analysis->image_size) {
            return unknown_value();
        }
        return constant_value(
            (uint16_t)analysis->image[(size_t)address.value] |
            ((uint16_t)analysis->image[(size_t)address.value + 1u] << 8u)
        );
    }
    if (strcmp(instruction->mnemonic, "ldis") == 0) {
        if ((size_t)address.value + 2u > analysis->image_size) {
            return unknown_value();
        }
        return constant_value(read_le16_signed(analysis->image + (size_t)address.value));
    }
    if ((size_t)address.value + 4u <= analysis->image_size) {
        return constant_value(read_le32(analysis->image + (size_t)address.value));
    }
    return unknown_value();
}

static vf2_abstract_value binary_result(
    const char *mnemonic,
    vf2_abstract_value left,
    vf2_abstract_value right
)
{
    const bool add = strcmp(mnemonic, "addo") == 0 ||
                     strcmp(mnemonic, "addi") == 0;
    const bool sub = strcmp(mnemonic, "subo") == 0 ||
                     strcmp(mnemonic, "subi") == 0;
    if (add) {
        if (left.kind == VF2_VALUE_CONSTANT && right.kind == VF2_VALUE_CONSTANT) {
            return constant_value(left.value + right.value);
        }
        if (left.kind == VF2_VALUE_STACK_RELATIVE && right.kind == VF2_VALUE_CONSTANT) {
            return stack_value(left.value + right.value);
        }
        if (right.kind == VF2_VALUE_STACK_RELATIVE && left.kind == VF2_VALUE_CONSTANT) {
            return stack_value(right.value + left.value);
        }
    }
    if (sub) {
        if (left.kind == VF2_VALUE_CONSTANT && right.kind == VF2_VALUE_CONSTANT) {
            return constant_value(right.value - left.value);
        }
        if (right.kind == VF2_VALUE_STACK_RELATIVE && left.kind == VF2_VALUE_CONSTANT) {
            return stack_value(right.value - left.value);
        }
    }
    if (left.kind == VF2_VALUE_CONSTANT && right.kind == VF2_VALUE_CONSTANT) {
        if (strcmp(mnemonic, "and") == 0) {
            return constant_value(left.value & right.value);
        }
        if (strcmp(mnemonic, "or") == 0) {
            return constant_value(left.value | right.value);
        }
        if (strcmp(mnemonic, "xor") == 0) {
            return constant_value(left.value ^ right.value);
        }
        if (strcmp(mnemonic, "shlo") == 0 || strcmp(mnemonic, "shli") == 0) {
            return constant_value((int64_t)((uint64_t)right.value << ((uint64_t)left.value & 31u)));
        }
        if (strcmp(mnemonic, "shro") == 0 || strcmp(mnemonic, "shri") == 0) {
            return constant_value((int64_t)((uint64_t)right.value >> ((uint64_t)left.value & 31u)));
        }
        if (strcmp(mnemonic, "mulo") == 0 || strcmp(mnemonic, "muli") == 0) {
            return constant_value(left.value * right.value);
        }
    }
    return unknown_value();
}

static void set_destination(
    register_state *state,
    const vf2_i960_operand *operand,
    vf2_abstract_value value
)
{
    if ((operand->kind == VF2_I960_OPERAND_REGISTER ||
         operand->kind == VF2_I960_OPERAND_FP_REGISTER ||
         operand->kind == VF2_I960_OPERAND_SPECIAL_REGISTER) &&
        operand->is_destination) {
        state->values[operand->value.reg] = value;
    }
}

static vf2_status transfer_instruction(
    vf2_i960_analysis *analysis,
    const vf2_i960_instruction *instruction,
    register_state *state,
    bool record_facts
)
{
    vf2_abstract_value result = unknown_value();
    const char *name = instruction->mnemonic;
    vf2_status status = VF2_OK;

    if (strcmp(name, "lda") == 0 && instruction->operand_count >= 2u) {
        result = evaluate_memory_address(&instruction->operands[0].value.memory, state);
        set_destination(state, &instruction->operands[1], result);
        if (record_facts) {
            status = add_constant_fact(
                analysis,
                instruction->address,
                instruction->operands[1].value.reg,
                result
            );
        }
    } else if ((name[0] == 'l' && name[1] == 'd') &&
               instruction->operand_count >= 2u &&
               instruction->operands[0].kind == VF2_I960_OPERAND_MEMORY) {
        result = load_value(analysis, instruction, state);
        set_destination(state, &instruction->operands[1], result);
        if (record_facts) {
            status = add_constant_fact(
                analysis,
                instruction->address,
                instruction->operands[1].value.reg,
                result
            );
        }
    } else if ((strcmp(name, "mov") == 0 || strcmp(name, "movr") == 0 ||
                strcmp(name, "movl") == 0 || strcmp(name, "movt") == 0 ||
                strcmp(name, "movq") == 0) && instruction->operand_count >= 2u) {
        result = evaluate_operand(&instruction->operands[0], state);
        set_destination(state, &instruction->operands[1], result);
        if (record_facts) {
            status = add_constant_fact(
                analysis,
                instruction->address,
                instruction->operands[1].value.reg,
                result
            );
        }
    } else if (instruction->operand_count >= 3u &&
               instruction->operands[2].is_destination) {
        const vf2_abstract_value left = evaluate_operand(&instruction->operands[0], state);
        const vf2_abstract_value right = evaluate_operand(&instruction->operands[1], state);
        result = binary_result(name, left, right);
        set_destination(state, &instruction->operands[2], result);
        if (record_facts) {
            status = add_constant_fact(
                analysis,
                instruction->address,
                instruction->operands[2].value.reg,
                result
            );
        }
    } else {
        size_t index = 0u;
        for (index = 0u; index < instruction->operand_count; ++index) {
            if (instruction->operands[index].is_destination &&
                instruction->operands[index].kind == VF2_I960_OPERAND_REGISTER) {
                state->values[instruction->operands[index].value.reg] = unknown_value();
            }
        }
    }

    if (instruction->flow == VF2_I960_FLOW_CALL) {
        uint8_t reg = VF2_I960_G0;
        for (reg = VF2_I960_G0; reg <= 30u; ++reg) {
            state->values[reg] = unknown_value();
        }
    }
    return status;
}

static size_t function_block_index(
    const vf2_i960_analysis *analysis,
    const vf2_function *function,
    uint32_t address
)
{
    size_t index = 0u;
    for (index = function->first_block;
         index < function->first_block + function->block_count;
         ++index) {
        if (analysis->blocks[index].start == address) {
            return index - function->first_block;
        }
    }
    return SIZE_MAX;
}

static vf2_basic_block *find_source_block(
    vf2_i960_analysis *analysis,
    const vf2_function *function,
    uint32_t address
)
{
    size_t index = 0u;
    for (index = function->first_block;
         index < function->first_block + function->block_count;
         ++index) {
        vf2_basic_block *block = &analysis->blocks[index];
        if (address >= block->start && address < block->end) {
            return block;
        }
    }
    return NULL;
}

static void add_block_successor(vf2_basic_block *block, uint32_t target)
{
    uint8_t index = 0u;
    if (block == NULL) {
        return;
    }
    for (index = 0u; index < block->successor_count; ++index) {
        if (block->successors[index] == target) {
            return;
        }
    }
    if (block->successor_count < VF2_MAX_BLOCK_SUCCESSORS) {
        block->successors[block->successor_count++] = target;
    }
}

static bool valid_target(const vf2_i960_analysis *analysis, uint32_t target)
{
    vf2_i960_instruction instruction;
    return (target & 3u) == 0u && (size_t)target + 4u <= analysis->image_size &&
           vf2_i960_decode(analysis->image, analysis->image_size, target, &instruction) == VF2_OK;
}

static vf2_status resolve_table(
    vf2_i960_analysis *analysis,
    vf2_function *function,
    const vf2_i960_instruction *instruction,
    uint32_t table_base
)
{
    uint32_t entry = 0u;
    size_t valid_count = 0u;
    size_t invalid_run = 0u;
    vf2_status status = VF2_OK;
    vf2_basic_block *block = find_source_block(analysis, function, instruction->address);
    const vf2_indirect_target_kind kind = instruction->flow == VF2_I960_FLOW_CALL
        ? VF2_INDIRECT_CALL
        : VF2_INDIRECT_JUMP_TABLE;

    for (entry = 0u; entry < VF2_MAX_TABLE_ENTRIES; ++entry) {
        const size_t offset = (size_t)table_base + (size_t)entry * 4u;
        uint32_t target = 0u;
        if (offset + 4u > analysis->image_size) {
            break;
        }
        target = read_le32(analysis->image + offset);
        if (!valid_target(analysis, target)) {
            ++invalid_run;
            if ((valid_count >= 2u && invalid_run >= 2u) ||
                (valid_count == 0u && invalid_run >= 4u)) {
                break;
            }
            continue;
        }
        invalid_run = 0u;
        ++valid_count;
        status = add_indirect_target(
            analysis,
            instruction->address,
            target,
            table_base,
            kind,
            85u
        );
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_i960_analysis_record_xref(
            analysis,
            instruction->address,
            target,
            instruction->flow == VF2_I960_FLOW_CALL
                ? VF2_XREF_CALL
                : VF2_XREF_BRANCH
        );
        if (status != VF2_OK) {
            return status;
        }
        if (instruction->flow == VF2_I960_FLOW_BRANCH) {
            add_block_successor(block, target);
        }
    }

    if (valid_count >= 2u) {
        function->resolved_indirect_count += valid_count;
        analysis->resolved_indirect_count += valid_count;
    } else {
        function->unresolved_indirect_count += 1u;
        analysis->unresolved_indirect_count += 1u;
    }
    return VF2_OK;
}

static vf2_status resolve_indirect(
    vf2_i960_analysis *analysis,
    vf2_function *function,
    const vf2_i960_instruction *instruction,
    const register_state *state
)
{
    vf2_abstract_value target;
    vf2_status status = VF2_OK;
    vf2_basic_block *block = NULL;
    if (!instruction->indirect || instruction->operand_count == 0u ||
        instruction->operands[0].kind != VF2_I960_OPERAND_MEMORY) {
        return VF2_OK;
    }
    target = evaluate_memory_address(&instruction->operands[0].value.memory, state);
    if (target.kind == VF2_VALUE_CONSTANT && target.value >= 0 &&
        target.value <= UINT32_MAX && valid_target(analysis, (uint32_t)target.value)) {
        const uint32_t address = (uint32_t)target.value;
        const vf2_indirect_target_kind kind = instruction->flow == VF2_I960_FLOW_CALL
            ? VF2_INDIRECT_CALL
            : VF2_INDIRECT_BRANCH;
        status = add_indirect_target(
            analysis,
            instruction->address,
            address,
            0u,
            kind,
            95u
        );
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_i960_analysis_record_xref(
            analysis,
            instruction->address,
            address,
            instruction->flow == VF2_I960_FLOW_CALL
                ? VF2_XREF_CALL
                : VF2_XREF_BRANCH
        );
        if (status != VF2_OK) {
            return status;
        }
        block = find_source_block(analysis, function, instruction->address);
        if (instruction->flow == VF2_I960_FLOW_BRANCH) {
            add_block_successor(block, address);
        }
        ++function->resolved_indirect_count;
        ++analysis->resolved_indirect_count;
        return VF2_OK;
    }
    if (target.kind == VF2_VALUE_TABLE_LOOKUP) {
        return resolve_table(analysis, function, instruction, target.table_base);
    }
    ++function->unresolved_indirect_count;
    ++analysis->unresolved_indirect_count;
    return VF2_OK;
}

static void inspect_register_usage(
    vf2_function *function,
    const vf2_i960_instruction *instruction,
    bool written[VF2_I960_REGISTER_COUNT],
    int64_t *max_stack_offset
)
{
    size_t index = 0u;
    for (index = 0u; index < instruction->operand_count; ++index) {
        const vf2_i960_operand *operand = &instruction->operands[index];
        if (operand->kind == VF2_I960_OPERAND_REGISTER ||
            operand->kind == VF2_I960_OPERAND_FP_REGISTER ||
            operand->kind == VF2_I960_OPERAND_SPECIAL_REGISTER) {
            const uint8_t reg = operand->value.reg;
            if (!operand->is_destination && reg >= VF2_I960_G0 &&
                reg <= VF2_I960_G7 && !written[reg]) {
                function->argument_register_mask |=
                    (uint16_t)(1u << (reg - VF2_I960_G0));
            }
            if (reg == VF2_I960_FP) {
                function->uses_frame_pointer = true;
            }
            if (operand->is_destination) {
                written[reg] = true;
            }
        } else if (operand->kind == VF2_I960_OPERAND_MEMORY) {
            const vf2_i960_memory_operand *memory = &operand->value.memory;
            if (memory->has_base && memory->base == VF2_I960_FP) {
                function->uses_frame_pointer = true;
            }
            if (memory->has_base &&
                (memory->base == VF2_I960_FP || memory->base == VF2_I960_SP)) {
                int64_t offset = memory->displacement;
                if (offset < 0) {
                    offset = -offset;
                }
                if (offset > *max_stack_offset) {
                    *max_stack_offset = offset;
                }
            }
        }
    }
    if (instruction->flow == VF2_I960_FLOW_CALL) {
        function->leaf = false;
    }
    if (instruction->flow == VF2_I960_FLOW_RETURN && written[VF2_I960_G0]) {
        function->return_register_mask |= 1u;
        function->has_return_value = true;
    }
}

static vf2_status analyze_function_semantics(
    vf2_i960_analysis *analysis,
    vf2_function *function
)
{
    const size_t count = function->block_count;
    register_state *inputs = NULL;
    bool *initialized = NULL;
    bool *queued = NULL;
    size_t *queue = NULL;
    size_t queue_count = 0u;
    size_t queue_cursor = 0u;
    bool written[VF2_I960_REGISTER_COUNT] = {false};
    int64_t max_stack_offset = 0;
    vf2_status status = VF2_OK;
    size_t index = 0u;

    function->leaf = true;
    function->uses_frame_pointer = false;
    function->has_return_value = false;
    function->stack_frame_size = 0u;
    function->argument_register_mask = 0u;
    function->return_register_mask = 0u;
    function->resolved_indirect_count = 0u;
    function->unresolved_indirect_count = 0u;

    if (count == 0u) {
        return VF2_OK;
    }
    inputs = (register_state *)calloc(count, sizeof(inputs[0]));
    initialized = (bool *)calloc(count, sizeof(initialized[0]));
    queued = (bool *)calloc(count, sizeof(queued[0]));
    queue = (size_t *)calloc(count * 8u + 1u, sizeof(queue[0]));
    if (inputs == NULL || initialized == NULL || queued == NULL || queue == NULL) {
        free(inputs);
        free(initialized);
        free(queued);
        free(queue);
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        inputs[0].values[index] = unknown_value();
    }
    inputs[0].values[VF2_I960_SP] = stack_value(0);
    inputs[0].values[VF2_I960_FP] = stack_value(0);
    for (index = VF2_I960_G0; index <= VF2_I960_G7; ++index) {
        inputs[0].values[index] = argument_value((uint8_t)index);
    }
    initialized[0] = true;
    queued[0] = true;
    queue[queue_count++] = 0u;

    while (queue_cursor < queue_count) {
        const size_t local_index = queue[queue_cursor++];
        const vf2_basic_block *block = &analysis->blocks[function->first_block + local_index];
        register_state state = inputs[local_index];
        uint32_t address = block->start;
        uint8_t successor = 0u;
        queued[local_index] = false;

        while (address < block->end) {
            vf2_i960_instruction instruction;
            if (vf2_i960_decode(
                    analysis->image,
                    analysis->image_size,
                    address,
                    &instruction
                ) != VF2_OK) {
                break;
            }
            status = transfer_instruction(analysis, &instruction, &state, false);
            if (status != VF2_OK) {
                goto cleanup;
            }
            address += instruction.size;
        }

        for (successor = 0u; successor < block->successor_count; ++successor) {
            const size_t next = function_block_index(
                analysis,
                function,
                block->successors[successor]
            );
            bool changed = false;
            if (next == SIZE_MAX) {
                continue;
            }
            if (!initialized[next]) {
                inputs[next] = state;
                initialized[next] = true;
                changed = true;
            } else {
                changed = merge_state(&inputs[next], &state);
            }
            if (changed && !queued[next] && queue_count < count * 8u + 1u) {
                queue[queue_count++] = next;
                queued[next] = true;
            }
        }
    }

    for (index = 0u; index < count; ++index) {
        const vf2_basic_block *block = &analysis->blocks[function->first_block + index];
        register_state state = initialized[index] ? inputs[index] : inputs[0];
        uint32_t address = block->start;
        while (address < block->end) {
            vf2_i960_instruction instruction;
            if (vf2_i960_decode(
                    analysis->image,
                    analysis->image_size,
                    address,
                    &instruction
                ) != VF2_OK) {
                break;
            }
            inspect_register_usage(function, &instruction, written, &max_stack_offset);
            status = resolve_indirect(analysis, function, &instruction, &state);
            if (status != VF2_OK) {
                goto cleanup;
            }
            status = transfer_instruction(analysis, &instruction, &state, true);
            if (status != VF2_OK) {
                goto cleanup;
            }
            address += instruction.size;
        }
    }

    if (max_stack_offset > 0) {
        const uint64_t aligned = ((uint64_t)max_stack_offset + 15u) & ~UINT64_C(15);
        function->stack_frame_size = aligned > UINT32_MAX
            ? UINT32_MAX
            : (uint32_t)aligned;
    }

cleanup:
    free(inputs);
    free(initialized);
    free(queued);
    free(queue);
    return status;
}

const char *vf2_value_kind_name(vf2_value_kind kind)
{
    switch (kind) {
    case VF2_VALUE_CONSTANT: return "constant";
    case VF2_VALUE_STACK_RELATIVE: return "stack-relative";
    case VF2_VALUE_ARGUMENT: return "argument";
    case VF2_VALUE_TABLE_LOOKUP: return "table-lookup";
    case VF2_VALUE_UNKNOWN:
    default: return "unknown";
    }
}

const char *vf2_indirect_target_kind_name(vf2_indirect_target_kind kind)
{
    switch (kind) {
    case VF2_INDIRECT_CALL: return "call";
    case VF2_INDIRECT_JUMP_TABLE: return "jump-table";
    case VF2_INDIRECT_BRANCH:
    default: return "branch";
    }
}

vf2_status vf2_i960_run_semantic_analysis(vf2_i960_analysis *analysis)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;
    if (analysis == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    free(analysis->constant_facts);
    free(analysis->indirect_targets);
    analysis->constant_facts = NULL;
    analysis->constant_fact_count = 0u;
    analysis->constant_fact_capacity = 0u;
    analysis->indirect_targets = NULL;
    analysis->indirect_target_count = 0u;
    analysis->indirect_target_capacity = 0u;
    analysis->resolved_indirect_count = 0u;
    analysis->unresolved_indirect_count = 0u;

    for (index = 0u; index < analysis->function_count; ++index) {
        status = analyze_function_semantics(analysis, &analysis->functions[index]);
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}
