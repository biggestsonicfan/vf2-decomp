#ifndef VF2_I960_INSTRUCTION_H
#define VF2_I960_INSTRUCTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum vf2_i960_format {
    VF2_I960_FORMAT_INVALID = 0,
    VF2_I960_FORMAT_CTRL,
    VF2_I960_FORMAT_COBR,
    VF2_I960_FORMAT_REG,
    VF2_I960_FORMAT_MEM
} vf2_i960_format;

typedef enum vf2_i960_operand_kind {
    VF2_I960_OPERAND_NONE = 0,
    VF2_I960_OPERAND_REGISTER,
    VF2_I960_OPERAND_FP_REGISTER,
    VF2_I960_OPERAND_SPECIAL_REGISTER,
    VF2_I960_OPERAND_LITERAL,
    VF2_I960_OPERAND_ADDRESS,
    VF2_I960_OPERAND_MEMORY
} vf2_i960_operand_kind;

typedef enum vf2_i960_flow {
    VF2_I960_FLOW_NONE = 0,
    VF2_I960_FLOW_BRANCH,
    VF2_I960_FLOW_CALL,
    VF2_I960_FLOW_RETURN,
    VF2_I960_FLOW_FAULT
} vf2_i960_flow;

typedef struct vf2_i960_memory_operand {
    bool has_base;
    bool has_index;
    bool ip_relative;
    bool absolute;
    uint8_t base;
    uint8_t index;
    uint8_t scale;
    int32_t displacement;
    uint32_t resolved_address;
} vf2_i960_memory_operand;

typedef struct vf2_i960_operand {
    vf2_i960_operand_kind kind;
    bool is_destination;
    union {
        uint8_t reg;
        int32_t literal;
        uint32_t address;
        vf2_i960_memory_operand memory;
    } value;
} vf2_i960_operand;

typedef struct vf2_i960_instruction {
    uint32_t address;
    uint32_t words[2];
    uint8_t size;
    uint16_t opcode;
    vf2_i960_format format;
    vf2_i960_flow flow;
    const char *mnemonic;
    vf2_i960_operand operands[3];
    uint8_t operand_count;
    bool valid;
    bool conditional;
    bool indirect;
    bool has_fallthrough;
    bool has_target;
    uint32_t target;
} vf2_i960_instruction;

const char *vf2_i960_register_name(uint8_t reg);
const char *vf2_i960_fp_register_name(uint8_t reg);
const char *vf2_i960_format_name(vf2_i960_format format);
const char *vf2_i960_flow_name(vf2_i960_flow flow);

#endif
