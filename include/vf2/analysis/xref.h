#ifndef VF2_ANALYSIS_XREF_H
#define VF2_ANALYSIS_XREF_H

#include <stdint.h>

typedef enum vf2_xref_type {
    VF2_XREF_CALL = 0,
    VF2_XREF_BRANCH,
    VF2_XREF_READ,
    VF2_XREF_WRITE,
    VF2_XREF_ADDRESS,
    VF2_XREF_STRING
} vf2_xref_type;

typedef struct vf2_xref {
    uint32_t source;
    uint32_t target;
    vf2_xref_type type;
} vf2_xref;

const char *vf2_xref_type_name(vf2_xref_type type);

#endif
