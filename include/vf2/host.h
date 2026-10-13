#ifndef VF2_HOST_H
#define VF2_HOST_H

/* SDL-free pieces of the host seam.  The SDL3 shell in src/host/host_sdl.c is
 * the only code that includes SDL headers; everything testable lives here. */

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

/* ---- input bindings ---------------------------------------------------- */

#define VF2_HOST_MAX_BINDINGS 128u

typedef struct vf2_host_binding {
    char source[40];   /* key name ("Z", "Left") or pad button ("pad:a") */
    uint32_t mask;     /* VF2_PLATFORM_BUTTON_* */
} vf2_host_binding;

typedef struct vf2_host_bindings {
    vf2_host_binding entries[VF2_HOST_MAX_BINDINGS];
    size_t count;
} vf2_host_bindings;

/* Button name ("PUNCH", "P2_KICK", ...) to mask; 0 if unknown. */
uint32_t vf2_host_button_from_name(const char *name);

void vf2_host_bindings_defaults(vf2_host_bindings *bindings);

/* Adds or replaces one binding.  Returns VF2_ERROR_INVALID_ARGUMENT for an
 * unknown button or an over-long source; never partially applies. */
vf2_status vf2_host_bindings_set(
    vf2_host_bindings *bindings, const char *source, const char *button
);

/* Parses "source = BUTTON" lines ('#' and ';' start comments) over the
 * current contents.  Fails closed on the first bad line and reports its
 * 1-based number through *bad_line (may be NULL). */
vf2_status vf2_host_bindings_parse(
    vf2_host_bindings *bindings, const char *text, size_t *bad_line
);

uint32_t vf2_host_bindings_lookup(
    const vf2_host_bindings *bindings, const char *source
);

/* Held-source tracker: press/release by source name, yields the mask. */
typedef struct vf2_host_input {
    uint32_t held_count[18];
    const vf2_host_bindings *bindings;
} vf2_host_input;

void vf2_host_input_init(vf2_host_input *input, const vf2_host_bindings *b);
void vf2_host_input_event(vf2_host_input *input, const char *source, int down);
uint32_t vf2_host_input_mask(const vf2_host_input *input);

/* ---- audio ring -------------------------------------------------------- */

/* Single-producer / single-consumer ring of stereo S16 frames.  Underrun is
 * filled with silence; overrun drops the newest frames.  Not lock-free on
 * purpose: the SDL shell guards it with the audio stream lock. */
typedef struct vf2_host_audio_ring {
    int16_t *data;     /* 2 * capacity samples */
    size_t capacity;   /* frames */
    size_t read;
    size_t count;
    uint64_t underruns;
    uint64_t dropped;
} vf2_host_audio_ring;

vf2_status vf2_host_audio_ring_init(vf2_host_audio_ring *ring, size_t frames);
void vf2_host_audio_ring_destroy(vf2_host_audio_ring *ring);
size_t vf2_host_audio_ring_write(
    vf2_host_audio_ring *ring, const int16_t *left, const int16_t *right,
    size_t frames
);
/* Writes exactly `frames` interleaved stereo frames to out. */
void vf2_host_audio_ring_read(
    vf2_host_audio_ring *ring, int16_t *out, size_t frames
);

/* ---- scaling ----------------------------------------------------------- */

typedef struct vf2_host_rect { int x, y, w, h; } vf2_host_rect;

/* Largest centred rectangle of the aspect src_w:src_h that fits the window;
 * with integer != 0 the scale is rounded down to a whole number (min 1). */
vf2_host_rect vf2_host_fit(
    int win_w, int win_h, int src_w, int src_h, int integer
);

#endif
