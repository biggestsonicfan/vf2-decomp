#include <stdint.h>
#include <string.h>

#include "vf2/analysis/cfg.h"
#include "vf2/analysis/pseudoc.h"
#include "vf2/i960/decoder.h"

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static int test_constant_indirect_branch(void)
{
    uint8_t image[96];
    uint32_t entry = 0u;
    vf2_i960_analysis analysis;
    vf2_status status = VF2_OK;
    size_t index = 0u;
    int found_constant = 0;

    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x8c883000u);
    write_le32(image + 4u, 0x00000020u);
    write_le32(image + 8u, 0x84045000u);
    write_le32(image + 0x20u, 0x0a000000u);

    status = vf2_i960_analysis_init(&analysis, image, sizeof(image));
    if (status != VF2_OK) {
        return 1;
    }
    status = vf2_i960_analyze(&analysis, &entry, 1u);
    if (status != VF2_OK) {
        vf2_i960_analysis_destroy(&analysis);
        return 2;
    }
    if (analysis.indirect_target_count != 1u ||
        analysis.indirect_targets[0].target != 0x20u ||
        analysis.resolved_indirect_count != 1u ||
        analysis.unresolved_indirect_count != 0u) {
        vf2_i960_analysis_destroy(&analysis);
        return 3;
    }
    {
        FILE *file = tmpfile();
        char generated[1024];
        size_t count = 0u;
        if (file == NULL ||
            vf2_i960_write_function_pseudoc(&analysis, 0u, file) != VF2_OK) {
            if (file != NULL) {
                fclose(file);
            }
            vf2_i960_analysis_destroy(&analysis);
            return 4;
        }
        rewind(file);
        count = fread(generated, 1u, sizeof(generated) - 1u, file);
        generated[count] = '\0';
        fclose(file);
        if (strstr(generated, "resolved indirect") == NULL) {
            vf2_i960_analysis_destroy(&analysis);
            return 5;
        }
    }
    for (index = 0u; index < analysis.constant_fact_count; ++index) {
        if (analysis.constant_facts[index].address == 0u &&
            analysis.constant_facts[index].reg == 17u &&
            analysis.constant_facts[index].value.kind == VF2_VALUE_CONSTANT &&
            analysis.constant_facts[index].value.value == 0x20) {
            found_constant = 1;
        }
    }
    vf2_i960_analysis_destroy(&analysis);
    return found_constant ? 0 : 6;
}

static int test_jump_table(void)
{
    uint8_t image[128];
    uint32_t entry = 0u;
    vf2_i960_analysis analysis;
    vf2_status status = VF2_OK;

    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x90883910u);
    write_le32(image + 4u, 0x00000040u);
    write_le32(image + 8u, 0x84045000u);
    write_le32(image + 0x20u, 0x0a000000u);
    write_le32(image + 0x24u, 0x0a000000u);
    write_le32(image + 0x40u, 0x00000020u);
    write_le32(image + 0x44u, 0x00000024u);
    write_le32(image + 0x48u, 0xffffffffu);
    write_le32(image + 0x4cu, 0xffffffffu);

    status = vf2_i960_analysis_init(&analysis, image, sizeof(image));
    if (status != VF2_OK) {
        return 1;
    }
    status = vf2_i960_analyze(&analysis, &entry, 1u);
    if (status != VF2_OK) {
        vf2_i960_analysis_destroy(&analysis);
        return 2;
    }
    if (analysis.indirect_target_count != 2u ||
        analysis.resolved_indirect_count != 2u ||
        analysis.indirect_targets[0].table_base != 0x40u ||
        analysis.indirect_targets[1].table_base != 0x40u) {
        vf2_i960_analysis_destroy(&analysis);
        return 3;
    }
    vf2_i960_analysis_destroy(&analysis);
    return 0;
}

int vf2_test_i960_semantics(void)
{
    const int constant_result = test_constant_indirect_branch();
    if (constant_result != 0) {
        return 10 + constant_result;
    }
    return test_jump_table();
}
