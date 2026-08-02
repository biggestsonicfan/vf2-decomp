#include "vf2/i960/decoder.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static vf2_status append_text(
    char *output,
    size_t output_size,
    size_t *position,
    const char *format,
    ...
)
{
    va_list arguments;
    int written = 0;

    if (*position >= output_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    va_start(arguments, format);
    written = vsnprintf(
        output + *position,
        output_size - *position,
        format,
        arguments
    );
    va_end(arguments);

    if (written < 0 || (size_t)written >= output_size - *position) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    *position += (size_t)written;
    return VF2_OK;
}

static vf2_status format_memory(
    const vf2_i960_memory_operand *memory,
    char *output,
    size_t output_size,
    size_t *position
)
{
    vf2_status status = VF2_OK;

    if (memory->ip_relative || memory->absolute) {
        status = append_text(
            output,
            output_size,
            position,
            "0x%08x",
            (unsigned)memory->resolved_address
        );
    } else if (memory->displacement != 0) {
        status = append_text(
            output,
            output_size,
            position,
            "0x%08x",
            (unsigned)(uint32_t)memory->displacement
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (memory->has_base) {
        status = append_text(
            output,
            output_size,
            position,
            "(%s)",
            vf2_i960_register_name(memory->base)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    if (memory->has_index) {
        if (memory->scale == 0u) {
            status = append_text(
                output,
                output_size,
                position,
                "[%s]",
                vf2_i960_register_name(memory->index)
            );
        } else {
            status = append_text(
                output,
                output_size,
                position,
                "[%s*%u]",
                vf2_i960_register_name(memory->index),
                1u << memory->scale
            );
        }
    }

    return status;
}

static vf2_status format_operand(
    const vf2_i960_operand *operand,
    char *output,
    size_t output_size,
    size_t *position
)
{
    switch (operand->kind) {
    case VF2_I960_OPERAND_REGISTER:
        return append_text(
            output,
            output_size,
            position,
            "%s",
            vf2_i960_register_name(operand->value.reg)
        );
    case VF2_I960_OPERAND_FP_REGISTER:
        return append_text(
            output,
            output_size,
            position,
            "%s",
            vf2_i960_fp_register_name(operand->value.reg)
        );
    case VF2_I960_OPERAND_SPECIAL_REGISTER:
        return append_text(
            output,
            output_size,
            position,
            "sf%u",
            (unsigned)operand->value.reg
        );
    case VF2_I960_OPERAND_LITERAL:
        return append_text(
            output,
            output_size,
            position,
            "%d",
            operand->value.literal
        );
    case VF2_I960_OPERAND_ADDRESS:
        return append_text(
            output,
            output_size,
            position,
            "0x%08x",
            (unsigned)operand->value.address
        );
    case VF2_I960_OPERAND_MEMORY:
        return format_memory(
            &operand->value.memory,
            output,
            output_size,
            position
        );
    case VF2_I960_OPERAND_NONE:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}

vf2_status vf2_i960_format_instruction(
    const vf2_i960_instruction *instruction,
    char *output,
    size_t output_size
)
{
    size_t position = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (instruction == NULL || output == NULL || output_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    output[0] = '\0';
    if (!instruction->valid) {
        return append_text(
            output,
            output_size,
            &position,
            ".word 0x%08x",
            (unsigned)instruction->words[0]
        );
    }

    status = append_text(
        output,
        output_size,
        &position,
        "%-9s",
        instruction->mnemonic
    );
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < instruction->operand_count; ++index) {
        if (index != 0u) {
            status = append_text(output, output_size, &position, ", ");
            if (status != VF2_OK) {
                return status;
            }
        }
        status = format_operand(
            &instruction->operands[index],
            output,
            output_size,
            &position
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    return VF2_OK;
}
