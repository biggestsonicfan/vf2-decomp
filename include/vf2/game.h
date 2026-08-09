#ifndef VF2_GAME_H
#define VF2_GAME_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/platform.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"
#include "vf2/tgp.h"

typedef struct vf2_game {
    unsigned frame_number;
    int initialized;
    vf2_platform *platform;
    vf2_tgp *tgp;
    vf2_sound_board *sound;
} vf2_game;

vf2_status vf2_game_initialize(vf2_game *game);

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
);

vf2_status vf2_game_set_input(vf2_game *game, uint32_t input);
vf2_status vf2_game_attach_audio(
    vf2_game *game,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
);
vf2_status vf2_game_begin_frame(vf2_game *game, uint32_t color);
vf2_status vf2_game_submit_geometry(
    vf2_game *game,
    const uint32_t *words,
    size_t word_count,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
);
vf2_status vf2_game_end_frame(vf2_game *game);
vf2_status vf2_game_read_pixels(
    const vf2_game *game,
    uint32_t *pixels,
    size_t pixel_count
);
vf2_status vf2_game_render_audio(
    vf2_game *game,
    int16_t *left,
    int16_t *right,
    size_t frames
);

vf2_status vf2_game_update(vf2_game *game);
void vf2_game_shutdown(vf2_game *game);

#endif
