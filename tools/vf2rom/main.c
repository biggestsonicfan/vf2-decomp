#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/boot.h"
#include "vf2/file.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

static void usage(const char *program)
{
    fprintf(
        stderr,
        "vf2rom v%s\n"
        "Usage:\n"
        "  %s verify <rom-directory>\n"
        "  %s info <rom-directory>\n"
        "  %s extract <rom-directory> <output-directory>\n"
        "  %s extract-region <rom-directory> <region> <output-file>\n"
        "  %s strings <rom-directory> <output-file> [minimum-length]\n"
        "  %s list-regions\n",
        VF2_VERSION_STRING,
        program,
        program,
        program,
        program,
        program,
        program
    );
}

static int command_verify(const char *rom_directory)
{
    vf2_verify_summary summary;
    vf2_status status = vf2_romset_verify(
        rom_directory,
        stdout,
        &summary
    );

    printf(
        "\nSummary: valid=%zu total=%zu missing=%zu "
        "bad-size=%zu bad-crc32=%zu bad-sha1=%zu\n",
        summary.valid,
        summary.total,
        summary.missing,
        summary.bad_size,
        summary.bad_crc32,
        summary.bad_sha1
    );

    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_info(const char *rom_directory)
{
    uint8_t *maincpu = NULL;
    size_t maincpu_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_status status = vf2_romset_build_region(
        rom_directory,
        VF2_REGION_MAINCPU,
        &maincpu,
        &maincpu_size
    );
    size_t index = 0u;

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Failed to build maincpu: %s\n",
            vf2_status_string(status)
        );
        return EXIT_FAILURE;
    }

    status = vf2_parse_i960_boot_vectors(
        maincpu,
        maincpu_size,
        &vectors
    );

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Failed to parse boot vectors: %s\n",
            vf2_status_string(status)
        );
        free(maincpu);
        return EXIT_FAILURE;
    }

    printf("Set: Virtua Fighter 2 Version 2.1, Model 2A\n");
    printf("Reconstructed maincpu size: 0x%zx bytes\n", maincpu_size);
    printf("System Address Table: 0x%08x\n",
           (unsigned)vectors.system_address_table);
    printf("Initial PRCB:         0x%08x\n",
           (unsigned)vectors.initial_prcb);
    printf("Reserved word:        0x%08x\n",
           (unsigned)vectors.reserved);
    printf("Initial instruction:  0x%08x\n",
           (unsigned)vectors.start_ip);
    printf("\nFirst eight vector words:\n");

    for (index = 0u; index < 8u; ++index) {
        printf(
            "  [0x%02zx] 0x%08x\n",
            index * 4u,
            (unsigned)vectors.raw_words[index]
        );
    }

    free(maincpu);
    return EXIT_SUCCESS;
}

static int command_extract(
    const char *rom_directory,
    const char *output_directory
)
{
    size_t index = 0u;
    vf2_status status = vf2_make_directories(output_directory);

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Could not create output directory: %s\n",
            vf2_status_string(status)
        );
        return EXIT_FAILURE;
    }

    for (index = 0u; index < VF2_REGION_COUNT; ++index) {
        char output_path[4096];
        const vf2_region_desc *region = &vf2_regions[index];

        status = vf2_join_path(
            output_path,
            sizeof(output_path),
            output_directory,
            region->output_filename
        );

        if (status != VF2_OK) {
            fprintf(stderr, "Output path is too long.\n");
            return EXIT_FAILURE;
        }

        status = vf2_romset_write_region(
            rom_directory,
            region->id,
            output_path
        );

        if (status != VF2_OK) {
            fprintf(
                stderr,
                "Failed to write %s: %s\n",
                region->name,
                vf2_status_string(status)
            );
            return EXIT_FAILURE;
        }

        printf(
            "[OK] %-18s -> %s (0x%zx bytes)\n",
            region->name,
            output_path,
            region->size
        );
    }

    return EXIT_SUCCESS;
}

static int command_extract_region(
    const char *rom_directory,
    const char *region_name,
    const char *output_path
)
{
    const vf2_region_desc *region = vf2_region_by_name(region_name);
    vf2_status status = VF2_OK;

    if (region == NULL) {
        fprintf(stderr, "Unknown region: %s\n", region_name);
        return EXIT_FAILURE;
    }

    status = vf2_romset_write_region(
        rom_directory,
        region->id,
        output_path
    );

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Failed to write region: %s\n",
            vf2_status_string(status)
        );
        return EXIT_FAILURE;
    }

    printf(
        "Wrote %s (0x%zx bytes) to %s\n",
        region->name,
        region->size,
        output_path
    );
    return EXIT_SUCCESS;
}

static int is_string_byte(unsigned char value)
{
    return value == '\t' || (value >= 0x20u && value <= 0x7eu);
}

static void print_escaped(FILE *output, const uint8_t *data, size_t size)
{
    size_t index = 0u;

    for (index = 0u; index < size; ++index) {
        if (data[index] == '\t') {
            fputs("\\t", output);
        } else if (data[index] == '\\') {
            fputs("\\\\", output);
        } else {
            fputc((int)data[index], output);
        }
    }
}

static int command_strings(
    const char *rom_directory,
    const char *output_path,
    size_t minimum_length
)
{
    uint8_t *maincpu = NULL;
    size_t maincpu_size = 0u;
    FILE *output = NULL;
    size_t index = 0u;
    vf2_status status = vf2_romset_build_region(
        rom_directory,
        VF2_REGION_MAINCPU,
        &maincpu,
        &maincpu_size
    );

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Failed to build maincpu: %s\n",
            vf2_status_string(status)
        );
        return EXIT_FAILURE;
    }

    output = fopen(output_path, "w");
    if (output == NULL) {
        fprintf(stderr, "Could not open %s\n", output_path);
        free(maincpu);
        return EXIT_FAILURE;
    }

    while (index < maincpu_size) {
        size_t start = index;

        while (index < maincpu_size &&
               is_string_byte(maincpu[index]) != 0) {
            ++index;
        }

        if (index - start >= minimum_length) {
            fprintf(output, "0x%08zx\t", start);
            print_escaped(output, maincpu + start, index - start);
            fputc('\n', output);
        }

        ++index;
    }

    if (fclose(output) != 0) {
        fprintf(stderr, "Could not finalize %s\n", output_path);
        free(maincpu);
        return EXIT_FAILURE;
    }

    free(maincpu);
    printf(
        "Extracted strings with minimum length %zu to %s\n",
        minimum_length,
        output_path
    );
    return EXIT_SUCCESS;
}

static int command_list_regions(void)
{
    size_t index = 0u;

    for (index = 0u; index < VF2_REGION_COUNT; ++index) {
        const vf2_region_desc *region = &vf2_regions[index];
        printf(
            "%-18s size=0x%08zx fill=0x%02x output=%s\n",
            region->name,
            region->size,
            (unsigned)region->fill,
            region->output_filename
        );
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "verify") == 0 && argc == 3) {
        return command_verify(argv[2]);
    }

    if (strcmp(argv[1], "info") == 0 && argc == 3) {
        return command_info(argv[2]);
    }

    if (strcmp(argv[1], "extract") == 0 && argc == 4) {
        return command_extract(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "extract-region") == 0 && argc == 5) {
        return command_extract_region(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "strings") == 0 &&
        (argc == 4 || argc == 5)) {
        size_t minimum_length = 5u;

        if (argc == 5) {
            char *end = NULL;
            unsigned long parsed = strtoul(argv[4], &end, 10);

            if (end == argv[4] || *end != '\0' || parsed == 0u) {
                fprintf(stderr, "Invalid minimum string length.\n");
                return EXIT_FAILURE;
            }

            minimum_length = (size_t)parsed;
        }

        return command_strings(
            argv[2],
            argv[3],
            minimum_length
        );
    }

    if (strcmp(argv[1], "list-regions") == 0 && argc == 2) {
        return command_list_regions();
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
