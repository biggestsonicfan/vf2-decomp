#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/boot.h"
#include "vf2/game.h"
#include "vf2/i960/snapshot.h"
#include "vf2/recovered.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s --rom-dir <directory> "
        "[--native-snapshot <snapshot>] [--frames <count>]\n",
        program
    );
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0u;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }
    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static vf2_status run_native_session(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t frame_count
)
{
    static const uint32_t width = 496u;
    static const uint32_t height = 384u;
    uint8_t *maincpu = NULL;
    uint8_t *main_data = NULL;
    uint8_t *tables = NULL;
    uint8_t *copro_data = NULL;
    uint8_t *polygons = NULL;
    uint8_t *audio_rom = NULL;
    uint8_t *sample_rom = NULL;
    size_t maincpu_size = 0u;
    size_t main_data_size = 0u;
    size_t tables_size = 0u;
    size_t copro_data_size = 0u;
    size_t polygons_size = 0u;
    size_t audio_size = 0u;
    size_t sample_size = 0u;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state runtime;
    vf2_i960_snapshot snapshot;
    vf2_game game = {0};
    vf2_native_runtime_run_report report;
    uint32_t *pixels = NULL;
    int16_t *left = NULL;
    int16_t *right = NULL;
    size_t pixel_count = (size_t)width * (size_t)height;
    size_t audio_frames = 735u;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&runtime, 0, sizeof(runtime));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&snapshot);

    status = vf2_romset_build_region(
        rom_directory, VF2_REGION_MAINCPU, &maincpu, &maincpu_size
    );
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_COPRO_TGP_TABLES, &tables, &tables_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_COPRO_DATA, &copro_data, &copro_data_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_POLYGONS, &polygons, &polygons_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_AUDIOCPU, &audio_rom, &audio_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_SAMPLES, &sample_rom, &sample_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_initialize(&machine) != 0
            ? VF2_OK : VF2_ERROR_OUT_OF_MEMORY;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_attach_main_rom(
            &machine, maincpu, maincpu_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_take_main_data(
            &machine, main_data, main_data_size
        );
        if (status == VF2_OK) {
            main_data = NULL;
        }
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_initialize(&runtime, 4u);
    }
    if (status == VF2_OK) {
        status = vf2_game_initialize(&game);
    }
    if (status == VF2_OK) {
        status = vf2_game_attach_graphics(
            &game, width, height, tables, tables_size, copro_data,
            copro_data_size, polygons, polygons_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_game_attach_audio(
            &game, audio_rom, audio_size, sample_rom, sample_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_game_attach_native_runtime(
            &game, &machine, &cpu, &runtime
        );
    }
    if (status == VF2_OK) {
        pixels = (uint32_t *)calloc(pixel_count, sizeof(*pixels));
        left = (int16_t *)calloc(audio_frames, sizeof(*left));
        right = (int16_t *)calloc(audio_frames, sizeof(*right));
        if (pixels == NULL || left == NULL || right == NULL) {
            status = VF2_ERROR_OUT_OF_MEMORY;
        }
    }
    for (uint32_t frame = 0u; status == VF2_OK && frame < frame_count; ++frame) {
        status = vf2_game_run_native_frame(&game, 100000u, &report);
        if (status == VF2_OK) {
            printf(
                "Native frame %u: blocks=%zu instructions=%llu ip=0x%08x\n",
                (unsigned)(frame + 1u), report.blocks_executed,
                (unsigned long long)report.recovered_instruction_count,
                (unsigned)report.final_address
            );
        } else {
            fprintf(
                stderr,
                "Native frame failed: %s blocks=%zu ip=0x%08x captured=%zu\n",
                vf2_status_string(status), report.blocks_executed,
                (unsigned)report.final_address, game.native_copro_word_count
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_game_read_pixels(&game, pixels, pixel_count);
    }
    if (status == VF2_OK) {
        status = vf2_game_render_audio(&game, left, right, audio_frames);
    }
    if (status == VF2_OK) {
        uint32_t checksum = 0u;
        for (size_t index = 0u; index < pixel_count; ++index) {
            checksum = (checksum * UINT32_C(16777619)) ^ pixels[index];
        }
        printf("Framebuffer checksum: 0x%08x\n", (unsigned)checksum);
        printf("Audio frames rendered: %zu\n", audio_frames);
    }

    free(pixels);
    free(left);
    free(right);
    vf2_game_shutdown(&game);
    vf2_i960_snapshot_destroy(&snapshot);
    vf2_model2a_shutdown(&machine);
    free(maincpu);
    free(main_data);
    free(tables);
    free(copro_data);
    free(polygons);
    free(audio_rom);
    free(sample_rom);
    return status;
}

int main(int argc, char **argv)
{
    const char *rom_directory = NULL;
    const char *native_snapshot = NULL;
    uint32_t native_frames = 1u;
    vf2_verify_summary summary;
    uint8_t *maincpu = NULL;
    size_t maincpu_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_recovered_boot_state recovered;
    vf2_status status = VF2_OK;

    if (argc < 3 || strcmp(argv[1], "--rom-dir") != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    rom_directory = argv[2];
    for (int index = 3; index < argc; ++index) {
        if (strcmp(argv[index], "--native-snapshot") == 0 &&
            index + 1 < argc) {
            native_snapshot = argv[++index];
        } else if (strcmp(argv[index], "--frames") == 0 &&
                   index + 1 < argc &&
                   parse_u32(argv[++index], &native_frames) &&
                   native_frames != 0u) {
            /* parsed */
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
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

    if (native_snapshot != NULL) {
        status = run_native_session(
            rom_directory, native_snapshot, native_frames
        );
        if (status != VF2_OK) {
            fprintf(stderr, "Native session failed: %s\n",
                    vf2_status_string(status));
            free(maincpu);
            return EXIT_FAILURE;
        }
    }

    free(maincpu);
    return EXIT_SUCCESS;
}
