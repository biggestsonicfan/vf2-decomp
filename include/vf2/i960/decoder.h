#ifndef VF2_I960_DECODER_H
#define VF2_I960_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/instruction.h"
#include "vf2/status.h"

vf2_status vf2_i960_decode(
    const uint8_t *image,
    size_t image_size,
    uint32_t address,
    vf2_i960_instruction *instruction
);

vf2_status vf2_i960_format_instruction(
    const vf2_i960_instruction *instruction,
    char *output,
    size_t output_size
);

#endif
