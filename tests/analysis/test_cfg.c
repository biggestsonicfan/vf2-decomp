#include <stdint.h>

#include "vf2/analysis/cfg.h"

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

int vf2_test_i960_cfg(void)
{
    uint8_t image[64] = {0xffu};
    uint32_t entry = 0u;
    vf2_i960_analysis analysis;
    vf2_status status = VF2_OK;

    write_le32(image + 0u, 0x09000010u);
    write_le32(image + 4u, 0x0800000cu);
    write_le32(image + 8u, 0x0a000000u);
    write_le32(image + 12u, 0x0a000000u);
    write_le32(image + 16u, 0x0a000000u);

    status = vf2_i960_analysis_init(&analysis, image, sizeof(image));
    if (status != VF2_OK) {
        return 1;
    }
    status = vf2_i960_analyze(&analysis, &entry, 1u);
    if (status != VF2_OK) {
        vf2_i960_analysis_destroy(&analysis);
        return 2;
    }
    if (analysis.function_count != 2u || analysis.block_count < 3u ||
        analysis.xref_count < 2u || analysis.function_split_count < 2u ||
        analysis.functions[0].tail_call_count < 1u ||
        vf2_i960_find_function(&analysis, 0x10u) == NULL) {
        vf2_i960_analysis_destroy(&analysis);
        return 3;
    }
    if (analysis.image_map[0] != VF2_IMAGE_CODE ||
        analysis.image_map[16] != VF2_IMAGE_CODE) {
        vf2_i960_analysis_destroy(&analysis);
        return 4;
    }

    vf2_i960_analysis_destroy(&analysis);
    return 0;
}
