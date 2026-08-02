#ifndef VF2_ROM_H
#define VF2_ROM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "vf2/status.h"

typedef enum vf2_load_type {
    VF2_LOAD32_WORD = 0,
    VF2_LOAD32_BYTE,
    VF2_LOAD16_WORD_SWAP
} vf2_load_type;

typedef enum vf2_region_id {
    VF2_REGION_MAINCPU = 0,
    VF2_REGION_MAIN_DATA,
    VF2_REGION_COPRO_DATA,
    VF2_REGION_POLYGONS,
    VF2_REGION_TEXTURES,
    VF2_REGION_AUDIOCPU,
    VF2_REGION_SAMPLES,
    VF2_REGION_COPRO_TGP_TABLES,
    VF2_REGION_OTHER_DATA,
    VF2_REGION_VIDEO_UNK,
    VF2_REGION_COUNT
} vf2_region_id;

typedef struct vf2_region_desc {
    vf2_region_id id;
    const char *name;
    const char *output_filename;
    size_t size;
    uint8_t fill;
} vf2_region_desc;

typedef struct vf2_rom_desc {
    const char *filename;
    vf2_region_id region;
    size_t offset;
    size_t size;
    vf2_load_type load_type;
    uint32_t crc32;
    const char *sha1;
    const char *description;
} vf2_rom_desc;

typedef struct vf2_verify_summary {
    size_t total;
    size_t valid;
    size_t missing;
    size_t bad_size;
    size_t bad_crc32;
    size_t bad_sha1;
} vf2_verify_summary;

extern const vf2_region_desc vf2_regions[VF2_REGION_COUNT];
extern const vf2_rom_desc vf2_roms[];
extern const size_t vf2_rom_count;

const char *vf2_load_type_name(vf2_load_type type);
const vf2_region_desc *vf2_region_by_name(const char *name);

vf2_status vf2_apply_rom_load(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    size_t offset,
    vf2_load_type type
);

vf2_status vf2_romset_verify(
    const char *rom_directory,
    FILE *report,
    vf2_verify_summary *summary
);

vf2_status vf2_romset_build_region(
    const char *rom_directory,
    vf2_region_id region,
    uint8_t **data_out,
    size_t *size_out
);

vf2_status vf2_romset_write_region(
    const char *rom_directory,
    vf2_region_id region,
    const char *output_path
);

#endif
