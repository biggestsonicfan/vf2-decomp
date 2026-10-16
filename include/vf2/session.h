#ifndef VF2_SESSION_H
#define VF2_SESSION_H

/* A session owns everything needed to run the recovered native runtime from a
 * proven i960 snapshot: ROM regions, machine, runtime and game.  Hosts (the
 * SDL3 shell, tests) talk to the core only through this header. */

#include <stddef.h>
#include <stdint.h>

#include "vf2/lift/runtime.h"
#include "vf2/model2a.h"
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

/* Render the 2D tile layer (Phase 7) from this session's tile RAM, font
 * (main_data ROM) and palette into `out`, a width x height buffer of
 * 0x00RRGGBB (width/height should be VF2_SESSION_WIDTH x VF2_SESSION_HEIGHT).
 * `palette_base` is the per-tile palette bank (its name-table attribute mapping
 * is not yet recovered; pass a text palette base, e.g. 0). Uses the recovered,
 * self-consistently validated tile pipeline (include/vf2/tile.h); on the attract
 * TEST MENU state it draws the real menu text. Returns VF2_ERROR_* on bad
 * arguments or geometry. */
vf2_status vf2_session_render_tile_layer(
    vf2_session *session,
    uint32_t *out,
    size_t width,
    size_t height,
    size_t palette_base
);

vf2_status vf2_session_render_audio(
    vf2_session *session,
    int16_t *left,
    int16_t *right,
    size_t frames
);

/* VF2_OK when the sound board attached; otherwise the reason it did not.
 * Without it vf2_session_render_audio returns silence. */
vf2_status vf2_session_audio_status(const vf2_session *session);

/* One pump report: bytes drained from the i960 sound queue through
 * the UART slot and filed on the board, ring entries assembled,
 * and their dispatch outcomes. `dispatched` reached the 0xcb2
 * copy-loop door with the slot program applied (E1b fall-through:
 * VF2_OK or UNSUPPORTED-at-door, the T2 round-1 shape) or
 * completed the key-on arm (E1c 0xed8: VF2_OK); `refused`
 * stopped earlier (a0 entry etc., silicon would still consume
 * the packet, so the pump advances past it the same way).
 * `drain_status` is the first non-OK drain or file status
 * (0xff reboot door, bus error), or VF2_OK when the queue
 * drained cleanly; a non-OK status stops the pump but still
 * returns VF2_OK -- the report carries the detail so one
 * corrupt queue byte cannot fail the frame. */
typedef struct vf2_session_audio_pump_report {
    uint32_t bytes_moved;
    uint32_t entries_filed;
    uint32_t entries_dispatched;
    uint32_t entries_refused;
    uint32_t drain_ticks;
    vf2_status drain_status;
} vf2_session_audio_pump_report;

/* Bridge one frame's worth of live sound: drain the i960 queue
 * (0x504001 ring, drainer 0x43800 semantics on a shadow CPU --
 * the session CPU is untouched) through the UART slot into the
 * board's L3 ring file, then dispatch each newly filed entry,
 * consuming refuse-doors the way the silicon drain epilogue does
 * (cursor advance regardless of outcome). Only live-filed queue
 * content flows; nothing is staged or seeded. Host-driven (call
 * after step, before render_audio), never implicit in step --
 * pumping mutates IO-control RAM and would otherwise perturb
 * oracle digests. Returns VF2_OK unless the session/audio is
 * unusable (bad argument, audio not attached). */
vf2_status vf2_session_pump_audio(
    vf2_session *session,
    vf2_session_audio_pump_report *report
);

/* FNV-1a of the work RAM (0x00500000..0x005fffff); determinism checks. */
vf2_status vf2_session_work_ram_digest(vf2_session *session, uint32_t *digest);

/* Raw work-RAM read for hunts (oracle-frame drivers): copies size
 * bytes from absolute address into destination (must lie fully in
 * 0x00500000..0x005fffff). Read-only; fails closed outside the
 * window. Used to read back queue cells / records a hunt located. */
vf2_status vf2_session_read_work_ram(
    vf2_session *session,
    uint32_t address,
    uint8_t *destination,
    size_t size
);

/* Passive bus observer for hunts (oracle-frame drivers). Forwards to
 * vf2_model2a_set_memory_observer on the session machine: observes
 * successful accesses only and never affects their result. NULL
 * detaches. Instrumentation must stay passive -- see the Model 2A
 * observer contract. */
vf2_status vf2_session_set_memory_observer(
    vf2_session *session,
    vf2_model2a_memory_observer observer,
    void *context
);

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

/* Step oracle frames until cpu.ip lands on stop_ip (checked before
 * every interpreter step) or max_frames complete. inputs[] cycles
 * per frame. Reports the hitting frame, steps taken, and g0/g13
 * at arrival (filer-input and record-pointer context). A clean
 * miss returns VF2_OK with hit == 0 -- a measured negative, not
 * a failure. Per-frame step cap mirrors step_oracle (fail closed).
 * Hunt primitive (T4): finds live filer/flusher arrivals without
 * staging queue state. */
typedef struct vf2_session_until_report {
    int hit;
    uint32_t frame;
    uint64_t steps;
    uint32_t g0;
    uint32_t g13;
    uint32_t ret_addr;
    uint32_t frames_completed;
} vf2_session_until_report;

vf2_status vf2_session_step_oracle_until(
    vf2_session *session,
    const uint32_t *inputs,
    size_t input_count,
    uint32_t stop_ip,
    uint32_t max_frames,
    vf2_session_until_report *report
);

void vf2_session_close(vf2_session *session);

#endif
