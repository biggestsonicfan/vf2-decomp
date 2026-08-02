#include "vf2/analysis/xref.h"

const char *vf2_xref_type_name(vf2_xref_type type)
{
    switch (type) {
    case VF2_XREF_CALL:
        return "call";
    case VF2_XREF_BRANCH:
        return "branch";
    case VF2_XREF_READ:
        return "read";
    case VF2_XREF_WRITE:
        return "write";
    case VF2_XREF_ADDRESS:
        return "address";
    case VF2_XREF_STRING:
        return "string";
    default:
        return "unknown";
    }
}
