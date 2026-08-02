#include "vf2/i960/instruction.h"

static const char *const register_names[32] = {
    "pfp", "sp", "rip", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
    "g8", "g9", "g10", "g11", "g12", "g13", "g14", "fp"
};

static const char *const fp_register_names[32] = {
    "fp0", "fp1", "fp2", "fp3", "sf4", "sf5", "sf6", "sf7",
    "sf8", "sf9", "sf10", "sf11", "sf12", "sf13", "sf14", "sf15",
    "+0.0", "sf17", "sf18", "sf19", "sf20", "sf21", "+1.0", "sf23",
    "sf24", "sf25", "sf26", "sf27", "sf28", "sf29", "sf30", "sf31"
};

const char *vf2_i960_register_name(uint8_t reg)
{
    return register_names[reg & 31u];
}

const char *vf2_i960_fp_register_name(uint8_t reg)
{
    return fp_register_names[reg & 31u];
}

const char *vf2_i960_format_name(vf2_i960_format format)
{
    switch (format) {
    case VF2_I960_FORMAT_CTRL:
        return "CTRL";
    case VF2_I960_FORMAT_COBR:
        return "COBR";
    case VF2_I960_FORMAT_REG:
        return "REG";
    case VF2_I960_FORMAT_MEM:
        return "MEM";
    case VF2_I960_FORMAT_INVALID:
    default:
        return "INVALID";
    }
}

const char *vf2_i960_flow_name(vf2_i960_flow flow)
{
    switch (flow) {
    case VF2_I960_FLOW_BRANCH:
        return "branch";
    case VF2_I960_FLOW_CALL:
        return "call";
    case VF2_I960_FLOW_RETURN:
        return "return";
    case VF2_I960_FLOW_FAULT:
        return "fault";
    case VF2_I960_FLOW_NONE:
    default:
        return "none";
    }
}
