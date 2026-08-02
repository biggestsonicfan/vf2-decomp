#include "vf2/rom.h"

#include <stdlib.h>
#include <string.h>

#include "vf2/file.h"
#include "vf2/hash.h"

const char *vf2_load_type_name(vf2_load_type type)
{
    switch (type) {
    case VF2_LOAD32_WORD:
        return "load32_word";
    case VF2_LOAD32_BYTE:
        return "load32_byte";
    case VF2_LOAD16_WORD_SWAP:
        return "load16_word_swap";
    default:
        return "unknown";
    }
}

const vf2_region_desc *vf2_region_by_name(const char *name)
{
    size_t index = 0u;

    if (name == NULL) {
        return NULL;
    }

    for (index = 0u; index < VF2_REGION_COUNT; ++index) {
        if (strcmp(vf2_regions[index].name, name) == 0) {
            return &vf2_regions[index];
        }
    }

    return NULL;
}

vf2_status vf2_apply_rom_load(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    size_t offset,
    vf2_load_type type
)
{
    size_t index = 0u;

    if (destination == NULL || source == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    switch (type) {
    case VF2_LOAD32_WORD:
        if ((source_size & 1u) != 0u) {
            return VF2_ERROR_BAD_SIZE;
        }

        if (source_size == 0u) {
            return VF2_OK;
        }

        if (offset + ((source_size / 2u - 1u) * 4u) + 2u >
            destination_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }

        for (index = 0u; index < source_size; index += 2u) {
            size_t destination_offset = offset + ((index / 2u) * 4u);
            destination[destination_offset] = source[index];
            destination[destination_offset + 1u] = source[index + 1u];
        }
        return VF2_OK;

    case VF2_LOAD32_BYTE:
        if (source_size == 0u) {
            return VF2_OK;
        }

        if (offset + ((source_size - 1u) * 4u) + 1u >
            destination_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }

        for (index = 0u; index < source_size; ++index) {
            destination[offset + (index * 4u)] = source[index];
        }
        return VF2_OK;

    case VF2_LOAD16_WORD_SWAP:
        if ((source_size & 1u) != 0u) {
            return VF2_ERROR_BAD_SIZE;
        }

        if (offset + source_size > destination_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }

        for (index = 0u; index < source_size; index += 2u) {
            destination[offset + index] = source[index + 1u];
            destination[offset + index + 1u] = source[index];
        }
        return VF2_OK;

    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}

vf2_status vf2_romset_verify(
    const char *rom_directory,
    FILE *report,
    vf2_verify_summary *summary
)
{
    vf2_verify_summary local_summary = {0u, 0u, 0u, 0u, 0u, 0u};
    size_t index = 0u;

    if (rom_directory == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    local_summary.total = vf2_rom_count;

    for (index = 0u; index < vf2_rom_count; ++index) {
        const vf2_rom_desc *rom = &vf2_roms[index];
        char path[4096];
        size_t actual_size = 0u;
        uint32_t actual_crc32 = 0u;
        uint8_t actual_sha1[VF2_SHA1_SIZE];
        char actual_sha1_hex[VF2_SHA1_HEX_SIZE];
        vf2_status status = vf2_join_path(
            path,
            sizeof(path),
            rom_directory,
            rom->filename
        );

        if (status != VF2_OK) {
            return status;
        }

        status = vf2_hash_file(
            path,
            &actual_size,
            &actual_crc32,
            actual_sha1
        );

        if (status != VF2_OK) {
            ++local_summary.missing;
            if (report != NULL) {
                fprintf(report, "[MISSING] %s\n", rom->filename);
            }
            continue;
        }

        if (actual_size != rom->size) {
            ++local_summary.bad_size;
            if (report != NULL) {
                fprintf(
                    report,
                    "[SIZE]    %s expected=%zu actual=%zu\n",
                    rom->filename,
                    rom->size,
                    actual_size
                );
            }
            continue;
        }

        if (actual_crc32 != rom->crc32) {
            ++local_summary.bad_crc32;
            if (report != NULL) {
                fprintf(
                    report,
                    "[CRC32]   %s expected=%08x actual=%08x\n",
                    rom->filename,
                    (unsigned)rom->crc32,
                    (unsigned)actual_crc32
                );
            }
            continue;
        }

        vf2_sha1_to_hex(actual_sha1, actual_sha1_hex);
        if (strcmp(actual_sha1_hex, rom->sha1) != 0) {
            ++local_summary.bad_sha1;
            if (report != NULL) {
                fprintf(
                    report,
                    "[SHA1]    %s expected=%s actual=%s\n",
                    rom->filename,
                    rom->sha1,
                    actual_sha1_hex
                );
            }
            continue;
        }

        ++local_summary.valid;
        if (report != NULL) {
            fprintf(report, "[OK]      %s\n", rom->filename);
        }
    }

    if (summary != NULL) {
        *summary = local_summary;
    }

    if (local_summary.valid == local_summary.total) {
        return VF2_OK;
    }

    if (local_summary.bad_size > 0u) {
        return VF2_ERROR_BAD_SIZE;
    }
    if (local_summary.bad_crc32 > 0u) {
        return VF2_ERROR_BAD_CRC32;
    }
    if (local_summary.bad_sha1 > 0u) {
        return VF2_ERROR_BAD_SHA1;
    }

    return VF2_ERROR_IO;
}

vf2_status vf2_romset_build_region(
    const char *rom_directory,
    vf2_region_id region,
    uint8_t **data_out,
    size_t *size_out
)
{
    const vf2_region_desc *region_desc = NULL;
    uint8_t *region_data = NULL;
    size_t index = 0u;

    if (rom_directory == NULL || data_out == NULL || size_out == NULL ||
        region < 0 || region >= VF2_REGION_COUNT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    region_desc = &vf2_regions[(size_t)region];
    region_data = (uint8_t *)malloc(region_desc->size);
    if (region_data == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    memset(region_data, region_desc->fill, region_desc->size);

    for (index = 0u; index < vf2_rom_count; ++index) {
        const vf2_rom_desc *rom = &vf2_roms[index];

        if (rom->region == region) {
            char path[4096];
            uint8_t *source = NULL;
            size_t source_size = 0u;
            vf2_status status = vf2_join_path(
                path,
                sizeof(path),
                rom_directory,
                rom->filename
            );

            if (status != VF2_OK) {
                free(region_data);
                return status;
            }

            status = vf2_read_file(path, &source, &source_size);
            if (status != VF2_OK) {
                free(region_data);
                return status;
            }

            if (source_size != rom->size) {
                free(source);
                free(region_data);
                return VF2_ERROR_BAD_SIZE;
            }

            status = vf2_apply_rom_load(
                region_data,
                region_desc->size,
                source,
                source_size,
                rom->offset,
                rom->load_type
            );

            free(source);

            if (status != VF2_OK) {
                free(region_data);
                return status;
            }
        }
    }

    *data_out = region_data;
    *size_out = region_desc->size;
    return VF2_OK;
}

vf2_status vf2_romset_write_region(
    const char *rom_directory,
    vf2_region_id region,
    const char *output_path
)
{
    uint8_t *data = NULL;
    size_t size = 0u;
    vf2_status status = VF2_OK;

    if (output_path == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_romset_build_region(
        rom_directory,
        region,
        &data,
        &size
    );

    if (status != VF2_OK) {
        return status;
    }

    status = vf2_write_file(output_path, data, size);
    free(data);
    return status;
}
