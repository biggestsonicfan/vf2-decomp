#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/boot.h"
#include "vf2/recovered.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s --rom-dir <directory>\n",
        program
    );
}

int main(int argc, char **argv)
{
    const char *rom_directory = NULL;
    vf2_verify_summary summary;
    uint8_t *maincpu = NULL;
    size_t maincpu_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_recovered_boot_state recovered;
    vf2_status status = VF2_OK;

    if (argc == 3 && strcmp(argv[1], "--rom-dir") == 0) {
        rom_directory = argv[2];
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("vf2-decomp v%s\n", VF2_VERSION_STRING);
    printf(
        "This release validates a repeated-frame recovered native runtime "
        "corridor; it is not yet a playable port.\n\n"
    );

    status = vf2_romset_verify(rom_directory, NULL, &summary);
    if (status != VF2_OK) {
        fprintf(
            stderr,
            "ROM verification failed: %s "
            "(valid=%zu missing=%zu size=%zu crc=%zu sha1=%zu)\n",
            vf2_status_string(status),
            summary.valid,
            summary.missing,
            summary.bad_size,
            summary.bad_crc32,
            summary.bad_sha1
        );
        return EXIT_FAILURE;
    }

    status = vf2_romset_build_region(
        rom_directory,
        VF2_REGION_MAINCPU,
        &maincpu,
        &maincpu_size
    );

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Could not build maincpu region: %s\n",
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
            "Could not parse i960 boot vectors: %s\n",
            vf2_status_string(status)
        );
        free(maincpu);
        return EXIT_FAILURE;
    }

    vf2_recovered_boot_entry(
        &recovered,
        vectors.system_address_table,
        vectors.initial_prcb,
        vectors.start_ip
    );

    printf("ROM set: Virtua Fighter 2 Version 2.1 (Model 2A)\n");
    printf("Files:   %zu/%zu valid\n", summary.valid, summary.total);
    printf("SAT:     0x%08x\n", (unsigned)recovered.system_address_table);
    printf("PRCB:    0x%08x\n", (unsigned)recovered.initial_prcb);
    printf("Entry:   0x%08x\n", (unsigned)recovered.initial_entry);
    printf("\nCurrent milestone: sixth-dispatch native runtime plus integrated graphics/audio boundaries.\n");
    printf(
        "Next milestone: recover remaining sound handlers and evidence-backed "
        "fighter/gameplay state.\n"
    );

    free(maincpu);
    return EXIT_SUCCESS;
}
