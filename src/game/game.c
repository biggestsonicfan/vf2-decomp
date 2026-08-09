#include "vf2/game.h"

#include <stdlib.h>
#include <string.h>

#include "vf2/geometry.h"

static vf2_status game_require_graphics(const vf2_game *game)
{
    if (game == NULL || game->initialized == 0 ||
        game->platform == NULL || game->tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status game_require_audio(const vf2_game *game)
{
    if (game == NULL || game->initialized == 0 || game->sound == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status game_require_native(const vf2_game *game)
{
    if (game == NULL || game->initialized == 0 ||
        game->native_machine == NULL || game->native_cpu == NULL ||
        game->native_runtime == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status game_render_native_geometry(vf2_game *game)
{
    uint32_t previous = 0u;
    uint32_t write = 0u;
    uint32_t distance = 0u;
    size_t word_count = 0u;
    uint32_t *words = NULL;
    vf2_tgp_geometry_stream_report report;
    vf2_status status = VF2_OK;

    if (game == NULL || game->native_machine == NULL ||
        game->platform == NULL || game->tgp == NULL) {
        return VF2_OK;
    }
    status = vf2_model2a_read_u32(
        game->native_machine,
        VF2_GEOMETRY_BASE + VF2_GEOMETRY_PREVIOUS_OFFSET,
        &previous
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            game->native_machine,
            VF2_GEOMETRY_BASE + VF2_GEOMETRY_WRITE_OFFSET,
            &write
        );
    }
    if (status != VF2_OK || previous < VF2_BUFFER_RAM_BASE ||
        previous >= VF2_BUFFER_RAM_BASE + VF2_BUFFER_RAM_SIZE ||
        write < VF2_BUFFER_RAM_BASE ||
        write >= VF2_BUFFER_RAM_BASE + VF2_BUFFER_RAM_SIZE) {
        return status != VF2_OK ? status : VF2_ERROR_UNSUPPORTED;
    }
    distance = write >= previous
        ? write - previous
        : (VF2_BUFFER_RAM_BASE + VF2_BUFFER_RAM_SIZE - previous) +
          (write - VF2_BUFFER_RAM_BASE);
    if ((distance & (sizeof(uint32_t) - 1u)) != 0u ||
        distance > VF2_BUFFER_RAM_SIZE) {
        return VF2_ERROR_UNSUPPORTED;
    }
    word_count = distance / sizeof(uint32_t);
    if (word_count == 0u) {
        return VF2_OK;
    }
    words = (uint32_t *)calloc(word_count, sizeof(*words));
    if (words == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    for (size_t index = 0u; index < word_count; ++index) {
        const uint32_t address = previous + (uint32_t)(index * sizeof(uint32_t));
        const uint32_t wrapped = address < VF2_BUFFER_RAM_BASE + VF2_BUFFER_RAM_SIZE
            ? address
            : VF2_BUFFER_RAM_BASE + (address - (VF2_BUFFER_RAM_BASE + VF2_BUFFER_RAM_SIZE));
        status = vf2_model2a_read_u32(game->native_machine, wrapped, &words[index]);
        if (status != VF2_OK) {
            break;
        }
    }
    if (status == VF2_OK) {
        status = vf2_tgp_execute_geometry_stream(
            game->tgp, words, word_count, game->platform,
            UINT32_C(0xffffffff), &report
        );
    }
    free(words);
    return status;
}

vf2_status vf2_game_initialize(vf2_game *game)
{
    if (game == 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    game->frame_number = 0u;
    game->initialized = 1;
    game->platform = NULL;
    game->tgp = NULL;
    game->sound = NULL;
    game->native_machine = NULL;
    game->native_cpu = NULL;
    game->native_runtime = NULL;
    memset(&game->native_report, 0, sizeof(game->native_report));
    return VF2_OK;
}

vf2_status vf2_game_attach_audio(
    vf2_game *game,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
)
{
    vf2_sound_board *sound;

    if (game == NULL || game->initialized == 0 || game->sound != NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    sound = (vf2_sound_board *)calloc(1u, sizeof(*sound));
    if (sound == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    vf2_sound_board_initialize(
        sound, audio_rom, audio_rom_size, sample_rom, sample_rom_size
    );
    game->sound = sound;
    return VF2_OK;
}

vf2_status vf2_game_attach_graphics(
    vf2_game *game,
    uint32_t width,
    uint32_t height,
    const uint8_t *tables,
    size_t tables_size,
    const uint8_t *copro_data,
    size_t copro_data_size,
    const uint8_t *polygon_rom,
    size_t polygon_rom_size
)
{
    vf2_platform *platform = NULL;
    vf2_tgp *tgp = NULL;
    vf2_status status = VF2_OK;

    if (game == NULL || game->initialized == 0 || width == 0u ||
        height == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (game->platform != NULL || game->tgp != NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    platform = (vf2_platform *)calloc(1u, sizeof(*platform));
    tgp = (vf2_tgp *)calloc(1u, sizeof(*tgp));
    if (platform == NULL || tgp == NULL) {
        free(platform);
        free(tgp);
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    status = vf2_platform_initialize(platform, width, height);
    if (status != VF2_OK) {
        free(platform);
        free(tgp);
        return status;
    }
    status = vf2_tgp_initialize(
        tgp, tables, tables_size, copro_data, copro_data_size
    );
    if (status != VF2_OK) {
        vf2_platform_shutdown(platform);
        free(platform);
        free(tgp);
        return status;
    }
    if (polygon_rom != NULL || polygon_rom_size != 0u) {
        status = vf2_tgp_attach_polygon_rom(
            tgp, polygon_rom, polygon_rom_size
        );
        if (status != VF2_OK) {
            vf2_platform_shutdown(platform);
            free(platform);
            free(tgp);
            return status;
        }
    }
    game->platform = platform;
    game->tgp = tgp;
    return VF2_OK;
}

vf2_status vf2_game_set_input(vf2_game *game, uint32_t input)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    return vf2_platform_set_input(game->platform, input);
}

vf2_status vf2_game_attach_native_runtime(
    vf2_game *game,
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *runtime
)
{
    if (game == NULL || game->initialized == 0 || machine == NULL ||
        cpu == NULL || runtime == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    game->native_machine = machine;
    game->native_cpu = cpu;
    game->native_runtime = runtime;
    memset(&game->native_report, 0, sizeof(game->native_report));
    return VF2_OK;
}

vf2_status vf2_game_run_native_frame(
    vf2_game *game,
    size_t max_blocks,
    vf2_native_runtime_run_report *report
)
{
    vf2_status status = game_require_native(game);
    int frame_open = 0;

    if (status != VF2_OK || max_blocks == 0u) {
        return status != VF2_OK ? status : VF2_ERROR_INVALID_ARGUMENT;
    }
    if (game->platform != NULL || game->tgp != NULL) {
        if (game->platform == NULL || game->tgp == NULL) {
            return VF2_ERROR_INVALID_ARGUMENT;
        }
        status = vf2_platform_begin_frame(game->platform, 0u);
        if (status != VF2_OK) {
            return status;
        }
        frame_open = 1;
    }
    status = vf2_native_runtime_run_frame(
        game->native_machine,
        game->native_cpu,
        game->native_runtime,
        max_blocks,
        &game->native_report
    );
    if (report != NULL) {
        *report = game->native_report;
    }
    if (status == VF2_OK && frame_open) {
        status = game_render_native_geometry(game);
    }
    if (frame_open) {
        vf2_status end_status = vf2_platform_end_frame(game->platform);
        if (status == VF2_OK) {
            status = end_status;
        }
    }
    if (status == VF2_OK) {
        ++game->frame_number;
    }
    return status;
}

vf2_status vf2_game_begin_frame(vf2_game *game, uint32_t color)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    return vf2_platform_begin_frame(game->platform, color);
}

vf2_status vf2_game_submit_geometry(
    vf2_game *game,
    const uint32_t *words,
    size_t word_count,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK || words == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_tgp_execute_geometry_stream(
        game->tgp, words, word_count, game->platform, color, report
    );
}

vf2_status vf2_game_end_frame(vf2_game *game)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    status = vf2_platform_end_frame(game->platform);
    if (status == VF2_OK) {
        ++game->frame_number;
    }
    return status;
}

vf2_status vf2_game_read_pixels(
    const vf2_game *game,
    uint32_t *pixels,
    size_t pixel_count
)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    return vf2_platform_read_pixels(game->platform, pixels, pixel_count);
}

vf2_status vf2_game_render_audio(
    vf2_game *game,
    int16_t *left,
    int16_t *right,
    size_t frames
)
{
    vf2_status status = game_require_audio(game);

    if (status != VF2_OK || left == NULL || right == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_sound_board_render(game->sound, left, right, frames);
}

vf2_status vf2_game_update(vf2_game *game)
{
    vf2_sound_voice_maintenance_report maintenance;
    vf2_sound_stream_maintenance_report stream_maintenance;

    if (game == 0 || game->initialized == 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (game->sound != NULL) {
        vf2_status status = vf2_sound_board_maintain_streams(
            game->sound, &stream_maintenance
        );
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_sound_board_maintain_voices(
            game->sound, &maintenance
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    if (game->native_machine != NULL || game->native_cpu != NULL ||
        game->native_runtime != NULL) {
        return vf2_game_run_native_frame(game, 100000u, NULL);
    }

    ++game->frame_number;
    return VF2_OK;
}

void vf2_game_shutdown(vf2_game *game)
{
    if (game != 0) {
        if (game->platform != NULL) {
            vf2_platform_shutdown(game->platform);
            free(game->platform);
        }
        free(game->tgp);
        free(game->sound);
        game->platform = NULL;
        game->tgp = NULL;
        game->sound = NULL;
        game->native_machine = NULL;
        game->native_cpu = NULL;
        game->native_runtime = NULL;
        memset(&game->native_report, 0, sizeof(game->native_report));
        game->initialized = 0;
    }
}
