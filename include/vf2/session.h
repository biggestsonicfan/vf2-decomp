#ifndef VF2_SESSION_H
#define VF2_SESSION_H

/* A session owns everything needed to run the recovered native runtime from a
 * proven i960 snapshot: ROM regions, machine, runtime and game.  Hosts (the
 * SDL3 shell, tests) talk to the core only through this header. */

#include <stddef.h>
#include <stdint.h>

#include "vf2/lift/runtime.h"
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

/* Runs one frame through the pure i960 interpreter oracle instead of the
 * recovered runtime (plan phase 2).  Same machine, ROM and start snapshot as
 * vf2_session_step and the same frame boundary, so vf2_session_work_ram_digest
 * taken after each is directly comparable: equal digests prove the recovered
 * runtime reproduces the oracle.  A session mixes the two steppers at its
 * peril; run one session per mode. */
vf2_status vf2_session_step_oracle(vf2_session *session, uint32_t input);

/* Like vf2_session_step_oracle, but dispatches each instruction through the
 * lifted>interpreter tiers of the recovered>lifted>interpreter registry, using
 * the caller-supplied lifted table (the generated vf2_lift_table /
 * vf2_lift_table_count when VF2_ENABLE_LIFTED is on; NULL/0 degrades to the
 * pure interpreter). The frame boundary and the work-RAM digest are identical
 * to vf2_session_step_oracle, so their per-frame digests must match exactly --
 * lifted reuses the interpreter semantics. What differs is the instruction
 * split: instructions retired by a lifted body count as lifted, not
 * interpreted (plan phase 4 / docs/VF2LIFT.md), so this is where a running
 * session's `interpreted` tally shrinks as the corridor is lifted. The
 * recovered tier (vf2_session_step's native runtime) overrides lifted, so the
 * caller omits recovered addresses from the table. */
vf2_status vf2_session_step_lifted(
    vf2_session *session,
    uint32_t input,
    const vf2_lift_entry *table,
    size_t count
);

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

/* Cumulative i960 instructions executed since the session opened, split into
 * the subset the interpreter decoded and ran (*interpreted) and the whole
 * (*executed).  Recovered C accounts for executed - interpreted.  The baseline
 * for Phase 4: a fully recovered session reaches interpreted == 0.  Either
 * pointer may be NULL. */
void vf2_session_instruction_counts(
    const vf2_session *session,
    uint64_t *executed,
    uint64_t *interpreted
);

void vf2_session_close(vf2_session *session);

#endif
