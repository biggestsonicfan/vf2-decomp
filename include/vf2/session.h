#ifndef VF2_SESSION_H
#define VF2_SESSION_H

/* A session owns everything needed to run the recovered native runtime from a
 * proven i960 snapshot: ROM regions, machine, runtime and game.  Hosts (the
 * SDL3 shell, tests) talk to the core only through this header. */

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

#define VF2_SESSION_WIDTH 496u
#define VF2_SESSION_HEIGHT 384u
#define VF2_SESSION_SAMPLE_RATE 44100u

typedef struct vf2_session vf2_session;

vf2_status vf2_session_open(
    const char *rom_directory,
    const char *snapshot_path,
    vf2_session **out_session
);

/* Runs one native frame with the given VF2_PLATFORM_BUTTON_* mask. */
vf2_status vf2_session_step(vf2_session *session, uint32_t input);

/* XRGB8888 pixels, VF2_SESSION_WIDTH x VF2_SESSION_HEIGHT, valid until the
 * next step.  Returns NULL if no frame has been produced. */
const uint32_t *vf2_session_pixels(vf2_session *session);

vf2_status vf2_session_render_audio(
    vf2_session *session,
    int16_t *left,
    int16_t *right,
    size_t frames
);

/* VF2_OK when the sound board attached; otherwise the reason it did not.
 * Without it vf2_session_render_audio returns silence. */
vf2_status vf2_session_audio_status(const vf2_session *session);

/* FNV-1a of the work RAM (0x00500000..0x005fffff); determinism checks. */
vf2_status vf2_session_work_ram_digest(vf2_session *session, uint32_t *digest);

/* The input mask last passed to the game (for host tests). */
uint32_t vf2_session_last_input(const vf2_session *session);

void vf2_session_close(vf2_session *session);

#endif
