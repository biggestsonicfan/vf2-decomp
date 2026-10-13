#include "vf2/session.h"

#include <stdlib.h>
#include <string.h>

#include "vf2/game.h"
#include "vf2/i960/snapshot.h"
#include "vf2/rom.h"

struct vf2_session {
    uint8_t *maincpu;
    uint8_t *tables;
    uint8_t *copro_data;
    uint8_t *polygons;
    uint8_t *audio_rom;
    uint8_t *sample_rom;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state runtime;
    vf2_i960_snapshot snapshot;
    vf2_game game;
    uint32_t *pixels;
    int have_frame;
    int machine_ready;
    uint32_t last_input;
};

static vf2_status build_region(
    const char *dir, vf2_region_id region, uint8_t **data, size_t *size
)
{
    return vf2_romset_build_region(dir, region, data, size);
}

vf2_status vf2_session_open(
    const char *rom_directory,
    const char *snapshot_path,
    vf2_session **out_session
)
{
    vf2_session *s = NULL;
    uint8_t *main_data = NULL;
    size_t maincpu_size = 0u, main_data_size = 0u, tables_size = 0u;
    size_t copro_size = 0u, poly_size = 0u, audio_size = 0u, sample_size = 0u;
    vf2_status st = VF2_OK;

    if (rom_directory == NULL || snapshot_path == NULL || out_session == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *out_session = NULL;
    s = (vf2_session *)calloc(1u, sizeof(*s));
    if (s == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    vf2_i960_snapshot_init(&s->snapshot);

    st = build_region(rom_directory, VF2_REGION_MAINCPU, &s->maincpu, &maincpu_size);
    if (st == VF2_OK) st = build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size);
    if (st == VF2_OK) st = build_region(rom_directory, VF2_REGION_COPRO_TGP_TABLES, &s->tables, &tables_size);
    if (st == VF2_OK) st = build_region(rom_directory, VF2_REGION_COPRO_DATA, &s->copro_data, &copro_size);
    if (st == VF2_OK) st = build_region(rom_directory, VF2_REGION_POLYGONS, &s->polygons, &poly_size);
    if (st == VF2_OK) st = build_region(rom_directory, VF2_REGION_AUDIOCPU, &s->audio_rom, &audio_size);
    if (st == VF2_OK) st = build_region(rom_directory, VF2_REGION_SAMPLES, &s->sample_rom, &sample_size);
    if (st == VF2_OK) {
        st = vf2_model2a_initialize(&s->machine) != 0
            ? VF2_OK : VF2_ERROR_OUT_OF_MEMORY;
        s->machine_ready = (st == VF2_OK);
    }
    if (st == VF2_OK) st = vf2_model2a_attach_main_rom(&s->machine, s->maincpu, maincpu_size);
    if (st == VF2_OK) {
        st = vf2_model2a_take_main_data(&s->machine, main_data, main_data_size);
        if (st == VF2_OK) main_data = NULL;
    }
    if (st == VF2_OK) st = vf2_i960_snapshot_read_file(&s->snapshot, snapshot_path);
    if (st == VF2_OK) st = vf2_i960_snapshot_restore(&s->snapshot, &s->cpu, &s->machine);
    if (st == VF2_OK) st = vf2_native_runtime_initialize(&s->runtime, 4u);
    if (st == VF2_OK) st = vf2_game_initialize(&s->game);
    if (st == VF2_OK) {
        st = vf2_game_attach_graphics(
            &s->game, VF2_SESSION_WIDTH, VF2_SESSION_HEIGHT, s->tables,
            tables_size, s->copro_data, copro_size, s->polygons, poly_size
        );
    }
    if (st == VF2_OK) {
        st = vf2_game_attach_audio(
            &s->game, s->audio_rom, audio_size, s->sample_rom, sample_size
        );
    }
    if (st == VF2_OK) {
        st = vf2_game_attach_native_runtime(
            &s->game, &s->machine, &s->cpu, &s->runtime
        );
    }
    if (st == VF2_OK) {
        s->pixels = (uint32_t *)calloc(
            (size_t)VF2_SESSION_WIDTH * VF2_SESSION_HEIGHT, sizeof(uint32_t)
        );
        if (s->pixels == NULL) st = VF2_ERROR_OUT_OF_MEMORY;
    }
    free(main_data);
    if (st != VF2_OK) {
        vf2_session_close(s);
        return st;
    }
    *out_session = s;
    return VF2_OK;
}

vf2_status vf2_session_step(vf2_session *s, uint32_t input)
{
    vf2_native_runtime_run_report report;
    vf2_status st;

    if (s == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    memset(&report, 0, sizeof(report));
    st = vf2_game_set_input(&s->game, input);
    if (st != VF2_OK) return st;
    s->last_input = input;
    st = vf2_game_run_native_frame(&s->game, 100000u, &report);
    if (st != VF2_OK) return st;
    st = vf2_game_read_pixels(
        &s->game, s->pixels, (size_t)VF2_SESSION_WIDTH * VF2_SESSION_HEIGHT
    );
    s->have_frame = (st == VF2_OK);
    return st;
}

const uint32_t *vf2_session_pixels(vf2_session *s)
{
    return (s != NULL && s->have_frame) ? s->pixels : NULL;
}

vf2_status vf2_session_render_audio(
    vf2_session *s, int16_t *left, int16_t *right, size_t frames
)
{
    if (s == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    return vf2_game_render_audio(&s->game, left, right, frames);
}

uint32_t vf2_session_last_input(const vf2_session *s)
{
    return s != NULL ? s->last_input : 0u;
}

void vf2_session_close(vf2_session *s)
{
    if (s == NULL) return;
    vf2_game_shutdown(&s->game);
    vf2_i960_snapshot_destroy(&s->snapshot);
    if (s->machine_ready) vf2_model2a_shutdown(&s->machine);
    free(s->pixels);
    free(s->maincpu);
    free(s->tables);
    free(s->copro_data);
    free(s->polygons);
    free(s->audio_rom);
    free(s->sample_rom);
    free(s);
}
