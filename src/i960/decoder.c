#include "vf2/i960/decoder.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct opcode_entry {
    uint16_t opcode;
    const char *name;
    int8_t flags;
} opcode_entry;

typedef enum mem_role {
    MEM_ROLE_LOAD = 0,
    MEM_ROLE_STORE,
    MEM_ROLE_ADDRESS,
    MEM_ROLE_BRANCH,
    MEM_ROLE_CALL,
    MEM_ROLE_CACHE
} mem_role;

typedef struct mem_entry {
    uint8_t opcode;
    const char *name;
    mem_role role;
} mem_entry;

static const char *const control_names[32] = {
    "b", "call", "ret", "bal", NULL, NULL, NULL, NULL,
    "bno", "bg", "be", "bge", "bl", "bne", "ble", "bo",
    "faultno", "faultg", "faulte", "faultge",
    "faultl", "faultne", "faultle", "faulto",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static const char *const cobr_names[32] = {
    "testno", "testg", "teste", "testge", "testl", "testne", "testle", "testo",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "bbc", "cmpobg", "cmpobe", "cmpobge", "cmpobl", "cmpobne", "cmpoble", "bbs",
    "cmpibno", "cmpibg", "cmpibe", "cmpibge", "cmpibl", "cmpibne", "cmpible", "cmpibo"
};

static const mem_entry memory_opcodes[] = {
    {0x80u, "ldob", MEM_ROLE_LOAD},
    {0x82u, "stob", MEM_ROLE_STORE},
    {0x84u, "bx", MEM_ROLE_BRANCH},
    {0x85u, "balx", MEM_ROLE_CALL},
    {0x86u, "callx", MEM_ROLE_CALL},
    {0x88u, "ldos", MEM_ROLE_LOAD},
    {0x8au, "stos", MEM_ROLE_STORE},
    {0x8cu, "lda", MEM_ROLE_ADDRESS},
    {0x90u, "ld", MEM_ROLE_LOAD},
    {0x92u, "st", MEM_ROLE_STORE},
    {0x98u, "ldl", MEM_ROLE_LOAD},
    {0x9au, "stl", MEM_ROLE_STORE},
    {0xa0u, "ldt", MEM_ROLE_LOAD},
    {0xa2u, "stt", MEM_ROLE_STORE},
    {0xadu, "dcinva", MEM_ROLE_CACHE},
    {0xb0u, "ldq", MEM_ROLE_LOAD},
    {0xb2u, "stq", MEM_ROLE_STORE},
    {0xc0u, "ldib", MEM_ROLE_LOAD},
    {0xc2u, "stib", MEM_ROLE_STORE},
    {0xc8u, "ldis", MEM_ROLE_LOAD},
    {0xcau, "stis", MEM_ROLE_STORE}
};

static const opcode_entry register_opcodes[] = {
    {0x580u, "notbit", -3}, {0x581u, "and", -3},
    {0x582u, "andnot", -3}, {0x583u, "setbit", -3},
    {0x584u, "notand", -3}, {0x586u, "xor", -3},
    {0x587u, "or", -3}, {0x588u, "nor", -3},
    {0x589u, "xnor", -3}, {0x58au, "not", -2},
    {0x58bu, "ornot", -3}, {0x58cu, "clrbit", -3},
    {0x58du, "notor", -3}, {0x58eu, "nand", -3},
    {0x58fu, "alterbit", -3},
    {0x590u, "addo", -3}, {0x591u, "addi", -3},
    {0x592u, "subo", -3}, {0x593u, "subi", -3},
    {0x594u, "cmpob", 2}, {0x595u, "cmpib", 2},
    {0x596u, "cmpos", 2}, {0x597u, "cmpis", 2},
    {0x598u, "shro", -3}, {0x59au, "shrdi", -3},
    {0x59bu, "shri", -3}, {0x59cu, "shlo", -3},
    {0x59du, "rotate", -3}, {0x59eu, "shli", -3},
    {0x5a0u, "cmpo", 2}, {0x5a1u, "cmpi", 2},
    {0x5a2u, "concmpo", 2}, {0x5a3u, "concmpi", 2},
    {0x5a4u, "cmpinco", -3}, {0x5a5u, "cmpinci", -3},
    {0x5a6u, "cmpdeco", -3}, {0x5a7u, "cmpdeci", -3},
    {0x5acu, "scanbyte", 2}, {0x5adu, "bswap", -2},
    {0x5aeu, "chkbit", 2},
    {0x5b0u, "addc", -3}, {0x5b2u, "subc", -3},
    {0x5b4u, "intdis", 0}, {0x5b5u, "inten", 0},
    {0x5ccu, "mov", -2}, {0x5d8u, "eshro", -3},
    {0x5dcu, "movl", -2}, {0x5ecu, "movt", -2},
    {0x5fcu, "movq", -2},
    {0x600u, "synmov", 2}, {0x601u, "synmovl", 2},
    {0x602u, "synmovq", 2}, {0x603u, "cmpstr", 3},
    {0x604u, "movqstr", -3}, {0x605u, "movstr", -3},
    {0x610u, "atmod", 33}, {0x612u, "atadd", 33},
    {0x613u, "inspacc", -2}, {0x614u, "ldphy", -2},
    {0x615u, "synld", -2}, {0x617u, "fill", 3},
    {0x630u, "sdma", 3}, {0x631u, "udma", 0},
    {0x640u, "spanbit", -2}, {0x641u, "scanbit", -2},
    {0x642u, "daddc", -3}, {0x643u, "dsubc", -3},
    {0x644u, "dmovt", -2}, {0x645u, "modac", 3},
    {0x650u, "modify", 33}, {0x651u, "extract", 33},
    {0x654u, "modtc", 33}, {0x655u, "modpc", 33},
    {0x656u, "receive", -2}, {0x658u, "intctl", -2},
    {0x659u, "sysctl", 33}, {0x65bu, "icctl", 33},
    {0x65cu, "dcctl", 33}, {0x65du, "halt", 0},
    {0x660u, "calls", 1}, {0x662u, "send", -3},
    {0x663u, "sendserv", 1}, {0x664u, "resumprcs", 1},
    {0x665u, "schedprcs", 1}, {0x666u, "saveprcs", 0},
    {0x668u, "condwait", 1}, {0x669u, "wait", 1},
    {0x66au, "signal", 1}, {0x66bu, "mark", 0},
    {0x66cu, "fmark", 0}, {0x66du, "flushreg", 0},
    {0x66fu, "syncf", 0},
    {0x670u, "emul", -3}, {0x671u, "ediv", -3},
    {0x674u, "cvtir", -20}, {0x675u, "cvtilr", -20},
    {0x676u, "scalerl", -30}, {0x677u, "scaler", -30},
    {0x680u, "atanr", -30}, {0x681u, "logepr", -30},
    {0x682u, "logr", -30}, {0x683u, "remr", -30},
    {0x684u, "cmpor", 20}, {0x685u, "cmpr", 20},
    {0x688u, "sqrtr", -20}, {0x689u, "expr", -20},
    {0x68au, "logbnr", -20}, {0x68bu, "roundr", -20},
    {0x68cu, "sinr", -20}, {0x68du, "cosr", -20},
    {0x68eu, "tanr", -20}, {0x68fu, "classr", 10},
    {0x690u, "atanrl", -30}, {0x691u, "logeprl", -30},
    {0x692u, "logrl", -30}, {0x693u, "remrl", -30},
    {0x694u, "cmporl", 20}, {0x695u, "cmprl", 20},
    {0x698u, "sqrtrl", -20}, {0x699u, "exprl", -20},
    {0x69au, "logbnrl", -20}, {0x69bu, "roundrl", -20},
    {0x69cu, "sinrl", -20}, {0x69du, "cosrl", -20},
    {0x69eu, "tanrl", -20}, {0x69fu, "classrl", 10},
    {0x6c0u, "cvtri", -20}, {0x6c1u, "cvtril", -20},
    {0x6c2u, "cvtzri", -20}, {0x6c3u, "cvtzril", -20},
    {0x6c9u, "movr", -20}, {0x6d9u, "movrl", -20},
    {0x6e1u, "movre", -20}, {0x6e2u, "cpysre", -30},
    {0x6e3u, "cpyrsre", -30}, {0x6e9u, "movre", -20},
    {0x701u, "mulo", -3}, {0x708u, "remo", -3},
    {0x70bu, "divo", -3}, {0x741u, "muli", -3},
    {0x748u, "remi", -3}, {0x749u, "modi", -3},
    {0x74bu, "divi", -3},
    {0x780u, "addono", -3}, {0x781u, "addino", -3},
    {0x782u, "subono", -3}, {0x783u, "subino", -3},
    {0x784u, "selno", -3}, {0x78bu, "divr", -30},
    {0x78cu, "mulr", -30}, {0x78du, "subr", -30},
    {0x78fu, "addr", -30},
    {0x790u, "addog", -3}, {0x791u, "addig", -3},
    {0x792u, "subog", -3}, {0x793u, "subig", -3},
    {0x794u, "selg", -3}, {0x79bu, "divrl", -30},
    {0x79cu, "mulrl", -30}, {0x79du, "subrl", -30},
    {0x79fu, "addrl", -30},
    {0x7a0u, "addoe", -3}, {0x7a1u, "addie", -3},
    {0x7a2u, "suboe", -3}, {0x7a3u, "subie", -3},
    {0x7a4u, "sele", -3},
    {0x7b0u, "addoge", -3}, {0x7b1u, "addige", -3},
    {0x7b2u, "suboge", -3}, {0x7b3u, "subige", -3},
    {0x7b4u, "selge", -3},
    {0x7c0u, "addol", -3}, {0x7c1u, "addil", -3},
    {0x7c2u, "subol", -3}, {0x7c3u, "subil", -3},
    {0x7c4u, "sell", -3},
    {0x7d0u, "addone", -3}, {0x7d1u, "addine", -3},
    {0x7d2u, "subone", -3}, {0x7d3u, "subine", -3},
    {0x7d4u, "selne", -3},
    {0x7e0u, "addole", -3}, {0x7e1u, "addile", -3},
    {0x7e2u, "subole", -3}, {0x7e3u, "subile", -3},
    {0x7e4u, "selle", -3},
    {0x7f0u, "addoo", -3}, {0x7f1u, "addio", -3},
    {0x7f2u, "suboo", -3}, {0x7f3u, "subio", -3},
    {0x7f4u, "selo", -3}
};

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static int32_t sign_extend(uint32_t value, unsigned bits)
{
    const uint32_t sign = UINT32_C(1) << (bits - 1u);
    const uint32_t mask = (UINT32_C(1) << bits) - UINT32_C(1);
    value &= mask;
    return (int32_t)((value ^ sign) - sign);
}

static void init_instruction(
    vf2_i960_instruction *instruction,
    uint32_t address,
    uint32_t word
)
{
    memset(instruction, 0, sizeof(*instruction));
    instruction->address = address;
    instruction->words[0] = word;
    instruction->size = 4u;
    instruction->mnemonic = ".word";
    instruction->has_fallthrough = true;
}

static vf2_i960_operand make_register(uint8_t reg, bool destination)
{
    vf2_i960_operand operand;
    memset(&operand, 0, sizeof(operand));
    operand.kind = VF2_I960_OPERAND_REGISTER;
    operand.is_destination = destination;
    operand.value.reg = reg;
    return operand;
}

static vf2_i960_operand make_fp_register(uint8_t reg, bool destination)
{
    vf2_i960_operand operand;
    memset(&operand, 0, sizeof(operand));
    operand.kind = VF2_I960_OPERAND_FP_REGISTER;
    operand.is_destination = destination;
    operand.value.reg = reg;
    return operand;
}

static vf2_i960_operand make_special_register(uint8_t reg, bool destination)
{
    vf2_i960_operand operand;
    memset(&operand, 0, sizeof(operand));
    operand.kind = VF2_I960_OPERAND_SPECIAL_REGISTER;
    operand.is_destination = destination;
    operand.value.reg = reg;
    return operand;
}

static vf2_i960_operand make_literal(int32_t value)
{
    vf2_i960_operand operand;
    memset(&operand, 0, sizeof(operand));
    operand.kind = VF2_I960_OPERAND_LITERAL;
    operand.value.literal = value;
    return operand;
}

static vf2_i960_operand make_address(uint32_t address)
{
    vf2_i960_operand operand;
    memset(&operand, 0, sizeof(operand));
    operand.kind = VF2_I960_OPERAND_ADDRESS;
    operand.value.address = address;
    return operand;
}

static vf2_i960_operand make_memory(vf2_i960_memory_operand memory)
{
    vf2_i960_operand operand;
    memset(&operand, 0, sizeof(operand));
    operand.kind = VF2_I960_OPERAND_MEMORY;
    operand.value.memory = memory;
    return operand;
}

static const opcode_entry *find_register_opcode(uint16_t opcode)
{
    size_t index = 0u;
    for (index = 0u;
         index < sizeof(register_opcodes) / sizeof(register_opcodes[0]);
         ++index) {
        if (register_opcodes[index].opcode == opcode) {
            return &register_opcodes[index];
        }
    }
    return NULL;
}

static const mem_entry *find_memory_opcode(uint8_t opcode)
{
    size_t index = 0u;
    for (index = 0u;
         index < sizeof(memory_opcodes) / sizeof(memory_opcodes[0]);
         ++index) {
        if (memory_opcodes[index].opcode == opcode) {
            return &memory_opcodes[index];
        }
    }
    return NULL;
}

static vf2_i960_operand decode_source(
    uint8_t value,
    bool literal,
    bool special
)
{
    if (special) {
        return make_special_register(value, false);
    }
    if (literal) {
        return make_literal((int32_t)value);
    }
    return make_register(value, false);
}

static vf2_status decode_control(vf2_i960_instruction *instruction)
{
    const uint8_t op = (uint8_t)(instruction->words[0] >> 24u);
    const unsigned table_index = (unsigned)op - 0x08u;
    const char *name = NULL;

    if (op < 0x08u || op > 0x27u || table_index >= 32u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    name = control_names[table_index];
    if (name == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    instruction->format = VF2_I960_FORMAT_CTRL;
    instruction->opcode = op;
    instruction->mnemonic = name;
    instruction->valid = true;

    if (op == 0x0au) {
        instruction->flow = VF2_I960_FLOW_RETURN;
        instruction->has_fallthrough = false;
        return VF2_OK;
    }

    if (op >= 0x18u) {
        instruction->flow = VF2_I960_FLOW_FAULT;
        instruction->conditional = true;
        instruction->has_fallthrough = false;
        return VF2_OK;
    }

    instruction->target = instruction->address +
        (uint32_t)sign_extend(instruction->words[0] & 0x00fffffcu, 24u);
    instruction->has_target = true;
    instruction->operands[0] = make_address(instruction->target);
    instruction->operand_count = 1u;

    if (op == 0x09u || op == 0x0bu) {
        instruction->flow = VF2_I960_FLOW_CALL;
        instruction->has_fallthrough = true;
    } else {
        instruction->flow = VF2_I960_FLOW_BRANCH;
        instruction->conditional = op >= 0x10u;
        instruction->has_fallthrough = instruction->conditional;
    }

    return VF2_OK;
}

static vf2_status decode_cobr(vf2_i960_instruction *instruction)
{
    const uint32_t word = instruction->words[0];
    const uint8_t op = (uint8_t)(word >> 24u);
    const unsigned table_index = (unsigned)op - 0x20u;
    const uint8_t src1 = (uint8_t)((word >> 19u) & 0x1fu);
    const uint8_t src2 = (uint8_t)((word >> 14u) & 0x1fu);
    const bool m1 = ((word >> 13u) & 1u) != 0u;
    const bool s2 = (word & 1u) != 0u;
    const char *name = NULL;

    if (op < 0x20u || op > 0x3fu || table_index >= 32u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    name = cobr_names[table_index];
    if (name == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    instruction->format = VF2_I960_FORMAT_COBR;
    instruction->opcode = op;
    instruction->mnemonic = name;
    instruction->valid = true;

    if (op <= 0x27u) {
        instruction->operands[0] = make_register(src1, true);
        instruction->operand_count = 1u;
        return VF2_OK;
    }

    instruction->operands[0] = decode_source(src1, m1, false);
    instruction->operands[1] = decode_source(src2, false, s2);
    instruction->target = instruction->address +
        (uint32_t)sign_extend(word & 0x1ffcu, 13u);
    instruction->operands[2] = make_address(instruction->target);
    instruction->operand_count = 3u;
    instruction->flow = VF2_I960_FLOW_BRANCH;
    instruction->conditional = true;
    instruction->has_target = true;
    instruction->has_fallthrough = true;
    return VF2_OK;
}

static vf2_status decode_register(vf2_i960_instruction *instruction)
{
    const uint32_t word = instruction->words[0];
    const uint16_t opcode = (uint16_t)(((word >> 20u) & 0xff0u) |
                                       ((word >> 7u) & 0x0fu));
    const opcode_entry *entry = find_register_opcode(opcode);
    const uint8_t destination = (uint8_t)((word >> 19u) & 0x1fu);
    const uint8_t src2 = (uint8_t)((word >> 14u) & 0x1fu);
    const bool m3 = ((word >> 13u) & 1u) != 0u;
    const bool m2 = ((word >> 12u) & 1u) != 0u;
    const bool m1 = ((word >> 11u) & 1u) != 0u;
    const bool s2 = ((word >> 6u) & 1u) != 0u;
    const bool s1 = ((word >> 5u) & 1u) != 0u;
    const uint8_t src1 = (uint8_t)(word & 0x1fu);
    int flags = 0;

    if (entry == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if ((s1 && m1) || (s2 && m2)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    instruction->format = VF2_I960_FORMAT_REG;
    instruction->opcode = opcode;
    instruction->mnemonic = entry->name;
    instruction->valid = true;
    flags = entry->flags;

    switch (flags) {
    case 0:
        break;
    case 1:
        instruction->operands[0] = decode_source(src1, m1, s1);
        instruction->operand_count = 1u;
        break;
    case -1:
        instruction->operands[0] = m3
            ? make_special_register(destination, true)
            : make_register(destination, true);
        instruction->operand_count = 1u;
        break;
    case 2:
        instruction->operands[0] = decode_source(src1, m1, s1);
        instruction->operands[1] = decode_source(src2, m2, s2);
        instruction->operand_count = 2u;
        break;
    case -2:
        instruction->operands[0] = decode_source(src1, m1, s1);
        instruction->operands[1] = m3
            ? make_special_register(destination, true)
            : make_register(destination, true);
        instruction->operand_count = 2u;
        break;
    case 3:
        instruction->operands[0] = decode_source(src1, m1, s1);
        instruction->operands[1] = decode_source(src2, m2, s2);
        instruction->operands[2] = m3
            ? make_literal((int32_t)destination)
            : make_register(destination, false);
        instruction->operand_count = 3u;
        break;
    case -3:
        instruction->operands[0] = decode_source(src1, m1, s1);
        instruction->operands[1] = decode_source(src2, m2, s2);
        instruction->operands[2] = m3
            ? make_special_register(destination, true)
            : make_register(destination, true);
        instruction->operand_count = 3u;
        break;
    case 33:
        if (m3) {
            return VF2_ERROR_UNSUPPORTED;
        }
        instruction->operands[0] = decode_source(src1, m1, s1);
        instruction->operands[1] = decode_source(src2, m2, s2);
        instruction->operands[2] = make_register(destination, true);
        instruction->operand_count = 3u;
        break;
    case 10:
        instruction->operands[0] = m1
            ? make_fp_register(src1, false)
            : make_register(src1, false);
        instruction->operand_count = 1u;
        break;
    case 20:
        instruction->operands[0] = m1
            ? make_fp_register(src1, false)
            : make_register(src1, false);
        instruction->operands[1] = m2
            ? make_fp_register(src2, false)
            : make_register(src2, false);
        instruction->operand_count = 2u;
        break;
    case -20:
        instruction->operands[0] = m1
            ? make_fp_register(src1, false)
            : make_register(src1, false);
        instruction->operands[1] = m3
            ? make_fp_register(destination, true)
            : make_register(destination, true);
        instruction->operand_count = 2u;
        break;
    case -30:
        instruction->operands[0] = m1
            ? make_fp_register(src1, false)
            : make_register(src1, false);
        instruction->operands[1] = m2
            ? make_fp_register(src2, false)
            : make_register(src2, false);
        instruction->operands[2] = m3
            ? make_fp_register(destination, true)
            : make_register(destination, true);
        instruction->operand_count = 3u;
        break;
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    if (opcode == 0x65du) {
        instruction->has_fallthrough = false;
    }
    return VF2_OK;
}

static vf2_status decode_memory(
    const uint8_t *image,
    size_t image_size,
    vf2_i960_instruction *instruction
)
{
    const uint32_t word = instruction->words[0];
    const uint8_t op = (uint8_t)(word >> 24u);
    const mem_entry *entry = find_memory_opcode(op);
    const uint8_t reg = (uint8_t)((word >> 19u) & 0x1fu);
    const uint8_t base = (uint8_t)((word >> 14u) & 0x1fu);
    const bool memb = ((word >> 12u) & 1u) != 0u;
    vf2_i960_memory_operand memory;
    uint32_t displacement_word = 0u;

    if (entry == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(&memory, 0, sizeof(memory));
    instruction->format = VF2_I960_FORMAT_MEM;
    instruction->opcode = op;
    instruction->mnemonic = entry->name;
    instruction->valid = true;

    if (!memb) {
        const bool has_base = ((word >> 13u) & 1u) != 0u;
        memory.displacement = (int32_t)(word & 0x0fffu);
        memory.has_base = has_base;
        memory.base = base;
        memory.absolute = !has_base;
        if (memory.absolute) {
            memory.resolved_address = (uint32_t)memory.displacement;
        }
    } else {
        const uint8_t mode = (uint8_t)((word >> 10u) & 0x0fu);
        const uint8_t scale = (uint8_t)((word >> 7u) & 0x07u);
        const uint8_t index = (uint8_t)(word & 0x1fu);

        if ((word & 0x60u) != 0u || scale > 4u || mode == 0x06u) {
            return VF2_ERROR_UNSUPPORTED;
        }

        memory.scale = scale;
        switch (mode) {
        case 0x04u:
            memory.has_base = true;
            memory.base = base;
            break;
        case 0x05u:
            if ((size_t)instruction->address + 8u > image_size) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            displacement_word = read_le32(image + instruction->address + 4u);
            instruction->words[1] = displacement_word;
            instruction->size = 8u;
            memory.ip_relative = true;
            memory.displacement = (int32_t)displacement_word;
            memory.resolved_address = instruction->address + 8u + displacement_word;
            break;
        case 0x07u:
            memory.has_base = true;
            memory.has_index = true;
            memory.base = base;
            memory.index = index;
            break;
        case 0x0cu:
        case 0x0du:
        case 0x0eu:
        case 0x0fu:
            if ((size_t)instruction->address + 8u > image_size) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            displacement_word = read_le32(image + instruction->address + 4u);
            instruction->words[1] = displacement_word;
            instruction->size = 8u;
            memory.displacement = (int32_t)displacement_word;
            memory.absolute = mode == 0x0cu;
            memory.has_base = mode == 0x0du || mode == 0x0fu;
            memory.has_index = mode == 0x0eu || mode == 0x0fu;
            memory.base = base;
            memory.index = index;
            if (memory.absolute) {
                memory.resolved_address = displacement_word;
            }
            break;
        default:
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    if (entry->role == MEM_ROLE_STORE) {
        instruction->operands[0] = make_register(reg, false);
        instruction->operands[1] = make_memory(memory);
        instruction->operands[1].is_destination = true;
        instruction->operand_count = 2u;
    } else if (entry->role == MEM_ROLE_BRANCH ||
               entry->role == MEM_ROLE_CALL ||
               entry->role == MEM_ROLE_CACHE) {
        instruction->operands[0] = make_memory(memory);
        instruction->operand_count = 1u;
        if (op == 0x85u) { /* balx has an explicit link-register destination. */
            instruction->operands[1] = make_register(reg, true);
            instruction->operand_count = 2u;
        }
    } else {
        instruction->operands[0] = make_memory(memory);
        instruction->operands[1] = make_register(reg, true);
        instruction->operand_count = 2u;
    }

    if (entry->role == MEM_ROLE_BRANCH || entry->role == MEM_ROLE_CALL) {
        instruction->flow = entry->role == MEM_ROLE_CALL
            ? VF2_I960_FLOW_CALL
            : VF2_I960_FLOW_BRANCH;
        instruction->indirect = !(memory.absolute || memory.ip_relative);
        if (!instruction->indirect) {
            instruction->target = memory.resolved_address;
            instruction->has_target = true;
        }
        instruction->has_fallthrough = entry->role == MEM_ROLE_CALL;

        /* The i960 ABI returns through g14. Sega's code commonly emits
         * `bx (g14)` instead of the dedicated `ret` instruction. */
        if (entry->role == MEM_ROLE_BRANCH && memory.has_base &&
            memory.base == 30u && !memory.has_index &&
            memory.displacement == 0 && !memory.absolute &&
            !memory.ip_relative) {
            instruction->flow = VF2_I960_FLOW_RETURN;
            instruction->indirect = false;
            instruction->has_target = false;
            instruction->has_fallthrough = false;
        }
    }

    return VF2_OK;
}

vf2_status vf2_i960_decode(
    const uint8_t *image,
    size_t image_size,
    uint32_t address,
    vf2_i960_instruction *instruction
)
{
    uint32_t word = 0u;
    uint8_t op = 0u;
    vf2_status status = VF2_ERROR_UNSUPPORTED;

    if (image == NULL || instruction == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((address & 3u) != 0u || (size_t)address + 4u > image_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    word = read_le32(image + address);
    op = (uint8_t)(word >> 24u);
    init_instruction(instruction, address, word);

    if (op >= 0x20u && op <= 0x3fu) {
        status = decode_cobr(instruction);
    } else if (op >= 0x08u && op <= 0x1fu) {
        status = decode_control(instruction);
    } else if (op >= 0x58u && op <= 0x7fu) {
        status = decode_register(instruction);
    } else {
        status = decode_memory(image, image_size, instruction);
    }

    return status;
}
