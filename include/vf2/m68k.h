#ifndef VF2_M68K_H
#define VF2_M68K_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

enum {
    VF2_M68K_MAX_INSTRUCTION_BYTES = 12u,
    VF2_M68K_TEXT_BYTES = 128u
};

typedef struct vf2_m68k_instruction {
    uint32_t address;
    uint16_t opcode;
    size_t length;
    int supported;
    char text[VF2_M68K_TEXT_BYTES];
} vf2_m68k_instruction;

vf2_status vf2_m68k_read_u16(
    const uint8_t *program,
    size_t program_size,
    uint32_t address,
    uint16_t *value
);

vf2_status vf2_m68k_read_u32(
    const uint8_t *program,
    size_t program_size,
    uint32_t address,
    uint32_t *value
);

vf2_status vf2_m68k_decode(
    const uint8_t *program,
    size_t program_size,
    uint32_t address,
    vf2_m68k_instruction *instruction
);

#endif
