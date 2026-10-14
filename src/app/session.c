#include "vf2/session.h"

#include <stdlib.h>
#include <string.h>

#include "vf2/game.h"
#include "vf2/hybrid/frame_wait.h"
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
    vf2_hybrid_frame_wait_state oracle_frame_wait;
    vf2_i960_snapshot snapshot;
    vf2_game game;
    uint32_t *pixels;
    int have_frame;
    int machine_ready;
    int audio_ready;
    vf2_status audio_status;
    uint32_t last_input;
    /* cpu.executed_instructions carried by the start snapshot (boot to the
     * cold dispatch).  Instruction counts are reported relative to this so the
     * split reflects only what this session ran.  interpreted starts at 0: it
     * is not part of the serialized snapshot. */
    uint64_t executed_baseline;
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
    if (st == VF2_OK) {
        st = vf2_hybrid_frame_wait_initialize(&s->oracle_frame_wait, 4u);
    }
    if (st == VF2_OK) st = vf2_game_initialize(&s->game);
    if (st == VF2_OK) {
        st = vf2_game_attach_graphics(
            &s->game, VF2_SESSION_WIDTH, VF2_SESSION_HEIGHT, s->tables,
            tables_size, s->copro_data, copro_size, s->polygons, poly_size
        );
    }
    if (st == VF2_OK) {
        /* Sound-board boot is real-ROM clean (v0825 S4a/8b: init OK
         * on the 0x80000 audio + 8MB sample regions; session-attach
         * leg pins attach + render determinism). No in-session
         * producer keys voices yet (8a hunt: unmapped), so session
         * audio renders init-state silence until the dispatch
         * bridge lands. Status is still reported for that reason. */
        s->audio_status = vf2_game_attach_audio(
            &s->game, s->audio_rom, audio_size, s->sample_rom, sample_size
        );
        s->audio_ready = (s->audio_status == VF2_OK);
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
    s->executed_baseline = s->cpu.executed_instructions;
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

vf2_status vf2_session_step_oracle(vf2_session *s, uint32_t input)
{
    /* HLE-TEMP (validation oracle; never shipped): advance one frame through
     * the pure i960 interpreter instead of the recovered runtime, driving the
     * frame interrupt with the same host-side scheduler the native path uses
     * (vf2_hybrid_frame_wait_observe, four visits to a frame-wait spin).  The
     * machine, ROM regions and start snapshot are those of this session, so a
     * per-frame work-RAM digest that equals vf2_session_step's digest proves
     * the recovered C reproduces the interpreter.  Returns with the CPU at the
     * vector-12 handler entry, exactly where vf2_session_step stops. */
    const uint64_t per_frame_budget = UINT64_C(50000000);
    uint64_t steps = 0u;
    vf2_status st;

    if (s == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    st = vf2_model2a_set_input(&s->machine, input);
    if (st != VF2_OK) return st;
    s->last_input = input;
    for (;;) {
        vf2_hybrid_frame_wait_report wait_report;
        st = vf2_hybrid_frame_wait_observe(
            &s->machine, &s->cpu, &s->oracle_frame_wait, &wait_report
        );
        if (st != VF2_OK) return st;
        if (wait_report.interrupt_injected) {
            return VF2_OK;
        }
        st = vf2_i960_step(&s->cpu, &s->machine, NULL);
        if (st != VF2_OK) return st;
        if (++steps >= per_frame_budget) return VF2_ERROR_UNSUPPORTED;
    }
}

const uint32_t *vf2_session_pixels(vf2_session *s)
{
    return (s != NULL && s->have_frame) ? s->pixels : NULL;
}

vf2_status vf2_session_render_audio(
    vf2_session *s, int16_t *left, int16_t *right, size_t frames
)
{
    if (s == NULL || left == NULL || right == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    if (!s->audio_ready) {
        memset(left, 0, frames * sizeof(*left));
        memset(right, 0, frames * sizeof(*right));
        return VF2_OK;
    }
    return vf2_game_render_audio(&s->game, left, right, frames);
}

vf2_status vf2_session_audio_status(const vf2_session *s)
{
    return s != NULL ? s->audio_status : VF2_ERROR_INVALID_ARGUMENT;
}

vf2_status vf2_session_work_ram_digest(vf2_session *s, uint32_t *digest)
{
    /* FNV-1a over 0x00500000..0x005fffff as little-endian words: the work RAM
     * the recovered runtime owns.  Used for determinism checks only. */
    uint32_t hash = UINT32_C(2166136261);
    if (s == NULL || digest == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    for (uint32_t a = UINT32_C(0x00500000); a < UINT32_C(0x00600000); a += 4u) {
        uint32_t w = 0u;
        vf2_status st = vf2_model2a_read_u32(&s->machine, a, &w);
        if (st != VF2_OK) return st;
        for (unsigned i = 0u; i < 4u; ++i) {
            hash = (hash ^ ((w >> (8u * i)) & 0xffu)) * UINT32_C(16777619);
        }
    }
    *digest = hash;
    return VF2_OK;
}

uint32_t vf2_session_last_input(const vf2_session *s)
{
    return s != NULL ? s->last_input : 0u;
}

void vf2_session_instruction_counts(
    const vf2_session *s, uint64_t *executed, uint64_t *interpreted
)
{
    if (executed != NULL) {
        *executed = s != NULL
            ? s->cpu.executed_instructions - s->executed_baseline
            : 0u;
    }
    if (interpreted != NULL) {
        *interpreted = s != NULL ? s->cpu.interpreted_instructions : 0u;
    }
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
