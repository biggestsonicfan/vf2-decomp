#include "vf2/analysis/pseudoc.h"

#include <stdio.h>
#include <string.h>

#include "vf2/analysis/cfg.h"
#include "vf2/analysis/symbols.h"
#include "vf2/file.h"
#include "vf2/i960/decoder.h"

static void append_text(char *buffer, size_t size, const char *text)
{
    const size_t length = strlen(buffer);
    if (length + 1u < size) {
        (void)snprintf(buffer + length, size - length, "%s", text);
    }
}

static void format_register(char *buffer, size_t size, uint8_t reg)
{
    (void)snprintf(buffer, size, "%s", vf2_i960_register_name(reg));
}

static void format_operand(
    const vf2_i960_operand *operand,
    char *buffer,
    size_t size
)
{
    buffer[0] = '\0';
    switch (operand->kind) {
    case VF2_I960_OPERAND_REGISTER:
    case VF2_I960_OPERAND_SPECIAL_REGISTER:
        format_register(buffer, size, operand->value.reg);
        break;
    case VF2_I960_OPERAND_FP_REGISTER:
        (void)snprintf(buffer, size, "%s", vf2_i960_fp_register_name(operand->value.reg));
        break;
    case VF2_I960_OPERAND_LITERAL:
        (void)snprintf(buffer, size, "%d", operand->value.literal);
        break;
    case VF2_I960_OPERAND_ADDRESS:
        (void)snprintf(buffer, size, "0x%08x", (unsigned)operand->value.address);
        break;
    case VF2_I960_OPERAND_MEMORY: {
        const vf2_i960_memory_operand *memory = &operand->value.memory;
        bool needs_plus = false;
        append_text(buffer, size, "MEM[");
        if (memory->absolute || memory->ip_relative) {
            char value[32];
            (void)snprintf(value, sizeof(value), "0x%08x", (unsigned)memory->resolved_address);
            append_text(buffer, size, value);
            needs_plus = true;
        } else if (memory->displacement != 0 || (!memory->has_base && !memory->has_index)) {
            char value[32];
            (void)snprintf(value, sizeof(value), "0x%08x", (unsigned)memory->displacement);
            append_text(buffer, size, value);
            needs_plus = true;
        }
        if (memory->has_base) {
            if (needs_plus) {
                append_text(buffer, size, " + ");
            }
            append_text(buffer, size, vf2_i960_register_name(memory->base));
            needs_plus = true;
        }
        if (memory->has_index) {
            char value[64];
            if (needs_plus) {
                append_text(buffer, size, " + ");
            }
            (void)snprintf(
                value,
                sizeof(value),
                "%s * %u",
                vf2_i960_register_name(memory->index),
                1u << memory->scale
            );
            append_text(buffer, size, value);
        }
        append_text(buffer, size, "]");
        break;
    }
    case VF2_I960_OPERAND_NONE:
    default:
        append_text(buffer, size, "?");
        break;
    }
}

static const char *binary_operator(const char *mnemonic)
{
    if (strcmp(mnemonic, "addo") == 0 || strcmp(mnemonic, "addi") == 0) return "+";
    if (strcmp(mnemonic, "subo") == 0 || strcmp(mnemonic, "subi") == 0) return "-";
    if (strcmp(mnemonic, "and") == 0) return "&";
    if (strcmp(mnemonic, "or") == 0) return "|";
    if (strcmp(mnemonic, "xor") == 0) return "^";
    if (strcmp(mnemonic, "shlo") == 0 || strcmp(mnemonic, "shli") == 0) return "<<";
    if (strcmp(mnemonic, "shro") == 0 || strcmp(mnemonic, "shri") == 0) return ">>";
    if (strcmp(mnemonic, "mulo") == 0 || strcmp(mnemonic, "muli") == 0) return "*";
    if (strcmp(mnemonic, "divo") == 0 || strcmp(mnemonic, "divi") == 0) return "/";
    if (strcmp(mnemonic, "remo") == 0 || strcmp(mnemonic, "remi") == 0) return "%";
    return NULL;
}

static const char *branch_condition(const char *mnemonic)
{
    if (strstr(mnemonic, "ne") != NULL) return "!=";
    if (strstr(mnemonic, "ge") != NULL) return ">=";
    if (strstr(mnemonic, "le") != NULL) return "<=";
    if (strstr(mnemonic, "bg") != NULL || strcmp(mnemonic, "bg") == 0) return ">";
    if (strstr(mnemonic, "bl") != NULL || strcmp(mnemonic, "bl") == 0) return "<";
    if (strstr(mnemonic, "be") != NULL || strcmp(mnemonic, "be") == 0) return "==";
    return NULL;
}

static size_t indirect_targets_for(
    const vf2_i960_analysis *analysis,
    uint32_t source,
    uint32_t *first_target
)
{
    size_t index = 0u;
    size_t count = 0u;
    for (index = 0u; index < analysis->indirect_target_count; ++index) {
        if (analysis->indirect_targets[index].source == source) {
            if (count == 0u && first_target != NULL) {
                *first_target = analysis->indirect_targets[index].target;
            }
            ++count;
        }
    }
    return count;
}

static void emit_instruction(
    const vf2_i960_analysis *analysis,
    const vf2_i960_instruction *instruction,
    FILE *output
)
{
    char operands[3][160];
    char assembly[256];
    size_t index = 0u;
    const char *op = binary_operator(instruction->mnemonic);
    for (index = 0u; index < instruction->operand_count; ++index) {
        format_operand(&instruction->operands[index], operands[index], sizeof(operands[index]));
    }

    if (instruction->flow == VF2_I960_FLOW_RETURN) {
        fputs("    return g0;\n", output);
        return;
    }
    if (instruction->flow == VF2_I960_FLOW_CALL) {
        uint32_t target = instruction->target;
        size_t count = 0u;
        if (!instruction->has_target) {
            count = indirect_targets_for(analysis, instruction->address, &target);
        } else {
            count = 1u;
        }
        if (count == 1u) {
            const char *target_name = vf2_i960_function_name(analysis, target);
            if (target_name != NULL) {
                fprintf(output, "    %s();\n", target_name);
            } else {
                fprintf(output, "    sub_%08x();\n", (unsigned)target);
            }
        } else if (count > 1u) {
            fprintf(output, "    /* indirect call: %zu candidate targets */\n", count);
        } else {
            fprintf(output, "    /* unresolved indirect call through %s */\n", operands[0]);
        }
        return;
    }
    if (instruction->flow == VF2_I960_FLOW_BRANCH) {
        if (strcmp(instruction->mnemonic, "b") == 0 && instruction->has_target) {
            fprintf(output, "    goto loc_%08x;\n", (unsigned)instruction->target);
            return;
        }
        if (instruction->has_target) {
            const char *condition = branch_condition(instruction->mnemonic);
            if (condition != NULL && instruction->operand_count >= 2u) {
                fprintf(
                    output,
                    "    if (%s %s %s) goto loc_%08x;\n",
                    operands[0],
                    condition,
                    operands[1],
                    (unsigned)instruction->target
                );
                return;
            }
            fprintf(
                output,
                "    /* %s */ goto loc_%08x;\n",
                instruction->mnemonic,
                (unsigned)instruction->target
            );
            return;
        }
        {
            uint32_t first = 0u;
            const size_t count = indirect_targets_for(analysis, instruction->address, &first);
            if (count == 1u) {
                fprintf(output, "    goto loc_%08x; /* resolved indirect */\n", (unsigned)first);
            } else if (count > 1u) {
                fprintf(output, "    /* switch/jump table with %zu targets */\n", count);
            } else {
                fprintf(output, "    /* unresolved indirect branch through %s */\n", operands[0]);
            }
            return;
        }
    }

    if (strcmp(instruction->mnemonic, "lda") == 0 && instruction->operand_count >= 2u) {
        fprintf(output, "    %s = ADDRESS_OF(%s);\n", operands[1], operands[0]);
        return;
    }
    if (instruction->mnemonic[0] == 'l' && instruction->mnemonic[1] == 'd' &&
        instruction->operand_count >= 2u) {
        fprintf(output, "    %s = READ_%s(%s);\n", operands[1], instruction->mnemonic, operands[0]);
        return;
    }
    if (instruction->mnemonic[0] == 's' && instruction->mnemonic[1] == 't' &&
        instruction->operand_count >= 2u) {
        fprintf(output, "    WRITE_%s(%s, %s);\n", instruction->mnemonic, operands[1], operands[0]);
        return;
    }
    if ((strcmp(instruction->mnemonic, "mov") == 0 ||
         strcmp(instruction->mnemonic, "movr") == 0) &&
        instruction->operand_count >= 2u) {
        fprintf(output, "    %s = %s;\n", operands[1], operands[0]);
        return;
    }
    if (op != NULL && instruction->operand_count >= 3u) {
        fprintf(
            output,
            "    %s = %s %s %s;\n",
            operands[2],
            operands[1],
            op,
            operands[0]
        );
        return;
    }

    if (vf2_i960_format_instruction(instruction, assembly, sizeof(assembly)) == VF2_OK) {
        fprintf(output, "    /* %s */\n", assembly);
    } else {
        fprintf(output, "    /* instruction at 0x%08x */\n", (unsigned)instruction->address);
    }
}

static void emit_signature(const vf2_function *function, FILE *output)
{
    uint8_t argument = 0u;
    bool first = true;
    fprintf(
        output,
        "%s %s(",
        function->has_return_value ? "uint32_t" : "void",
        function->name
    );
    for (argument = 0u; argument < 8u; ++argument) {
        if ((function->argument_register_mask & (uint16_t)(1u << argument)) != 0u) {
            fprintf(output, "%suint32_t g%u", first ? "" : ", ", argument);
            first = false;
        }
    }
    if (first) {
        fputs("void", output);
    }
    fputs(")\n", output);
}

vf2_status vf2_i960_write_function_pseudoc(
    const vf2_i960_analysis *analysis,
    uint32_t function_address,
    FILE *output
)
{
    const vf2_function *function = NULL;
    size_t block_index = 0u;
    if (analysis == NULL || output == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    function = vf2_i960_find_function(analysis, function_address);
    if (function == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    fputs("#include <stdint.h>\n\n", output);
    fprintf(
        output,
        "/* Automatically generated non-matching pseudocode.\n"
        " * Address: 0x%08x\n"
        " * Stack estimate: 0x%x bytes\n"
        " * Indirect flow: %zu resolved, %zu unresolved\n"
        " */\n",
        (unsigned)function->address,
        (unsigned)function->stack_frame_size,
        function->resolved_indirect_count,
        function->unresolved_indirect_count
    );
    emit_signature(function, output);
    fputs("{\n", output);
    fputs("    uint32_t pfp = 0, sp = 0, rip = 0, r3 = 0, r4 = 0, r5 = 0;\n", output);
    fputs("    uint32_t r6 = 0, r7 = 0, r8 = 0, r9 = 0, r10 = 0, r11 = 0;\n", output);
    fputs("    uint32_t r12 = 0, r13 = 0, r14 = 0, r15 = 0, fp = 0;\n", output);
    {
        uint8_t global = 0u;
        for (global = 0u; global < 8u; ++global) {
            if ((function->argument_register_mask & (uint16_t)(1u << global)) == 0u) {
                fprintf(output, "    uint32_t g%u = 0;\n", (unsigned)global);
            }
        }
    }
    fputs("    uint32_t g8 = 0, g9 = 0, g10 = 0, g11 = 0, g12 = 0, g13 = 0, g14 = 0;\n", output);
    (void)fprintf(output, "    (void)pfp; (void)sp; (void)rip; (void)fp;\n");

    for (block_index = function->first_block;
         block_index < function->first_block + function->block_count;
         ++block_index) {
        const vf2_basic_block *block = &analysis->blocks[block_index];
        uint32_t address = block->start;
        fprintf(output, "\nloc_%08x:\n", (unsigned)block->start);
        while (address < block->end) {
            vf2_i960_instruction instruction;
            if (vf2_i960_decode(
                    analysis->image,
                    analysis->image_size,
                    address,
                    &instruction
                ) != VF2_OK) {
                fprintf(output, "    /* decode failure at 0x%08x */\n", (unsigned)address);
                break;
            }
            emit_instruction(analysis, &instruction, output);
            address += instruction.size;
        }
    }
    if (!function->has_return_value) {
        fputs("    return;\n", output);
    }
    fputs("}\n", output);
    return VF2_OK;
}

vf2_status vf2_i960_write_all_pseudoc(
    const vf2_i960_analysis *analysis,
    const char *output_directory
)
{
    char directory[4096];
    size_t index = 0u;
    vf2_status status = VF2_OK;
    if (analysis == NULL || output_directory == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_join_path(directory, sizeof(directory), output_directory, "pseudo-c");
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_make_directories(directory);
    if (status != VF2_OK) {
        return status;
    }
    for (index = 0u; index < analysis->function_count; ++index) {
        char filename[64];
        char path[4096];
        FILE *file = NULL;
        (void)snprintf(
            filename,
            sizeof(filename),
            "sub_%08x.c",
            (unsigned)analysis->functions[index].address
        );
        status = vf2_join_path(path, sizeof(path), directory, filename);
        if (status != VF2_OK) {
            return status;
        }
        file = fopen(path, "wb");
        if (file == NULL) {
            return VF2_ERROR_IO;
        }
        status = vf2_i960_write_function_pseudoc(
            analysis,
            analysis->functions[index].address,
            file
        );
        if (fclose(file) != 0 && status == VF2_OK) {
            status = VF2_ERROR_IO;
        }
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}
