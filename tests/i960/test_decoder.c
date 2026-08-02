#include <stdint.h>
#include <string.h>

#include "vf2/i960/decoder.h"

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

int vf2_test_i960_decoder(void)
{
    uint8_t image[32] = {0u};
    vf2_i960_instruction instruction;
    char text[128];

    write_le32(image + 0u, 0x8c803000u);
    write_le32(image + 4u, 0x00e00000u);
    if (vf2_i960_decode(image, sizeof(image), 0u, &instruction) != VF2_OK) {
        return 1;
    }
    if (strcmp(instruction.mnemonic, "lda") != 0 || instruction.size != 8u ||
        instruction.operand_count != 2u ||
        instruction.operands[0].value.memory.resolved_address != 0x00e00000u) {
        return 2;
    }
    if (vf2_i960_format_instruction(&instruction, text, sizeof(text)) != VF2_OK ||
        strstr(text, "0x00e00000") == NULL || strstr(text, "g0") == NULL) {
        return 3;
    }

    write_le32(image + 8u, 0x59981901u);
    if (vf2_i960_decode(image, sizeof(image), 8u, &instruction) != VF2_OK ||
        strcmp(instruction.mnemonic, "subo") != 0) {
        return 4;
    }

    write_le32(image + 12u, 0x12000014u);
    if (vf2_i960_decode(image, sizeof(image), 12u, &instruction) != VF2_OK ||
        strcmp(instruction.mnemonic, "be") != 0 ||
        instruction.target != 0x20u || !instruction.conditional) {
        return 5;
    }

    write_le32(image + 16u, 0x0a000000u);
    if (vf2_i960_decode(image, sizeof(image), 16u, &instruction) != VF2_OK ||
        instruction.flow != VF2_I960_FLOW_RETURN || instruction.has_fallthrough) {
        return 6;
    }

    return 0;
}
