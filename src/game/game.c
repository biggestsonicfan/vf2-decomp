#include "vf2/game.h"

#include <stdlib.h>

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
        game->initialized = 0;
    }
}
