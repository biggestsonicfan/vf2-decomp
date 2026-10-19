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

/* Explicit opt-in for the owner-ROM-verified D92->DF8->ED6 SCSP
 * upload/KEYON route inside the regular session audio pump.
 * Disabled by default. Enabling does not manufacture game events;
 * original i960/UART packets still must be genuinely present.
 * Unsupported/invalid ROM or channel states remain fail-closed. */
vf2_status vf2_session_set_verified_d92_copy(
    vf2_session *session,
    int enabled
);

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
 * corrupt queue byte cannot fail the frame. `stream_ticks` /
 * `voice_ticks` count the autonomous per-frame board ticks run
 * after the dispatches (L2 maintain_streams, L1
 * maintain_voices; 1 each per pump). Their statuses are
 * VF2_OK when the tick completed; a tick refusal (unmeasured
 * live stream record) is reported without failing the frame,
 * the same way a drain refusal is. */
typedef struct vf2_session_audio_pump_report {
    uint32_t bytes_moved;
    uint32_t entries_filed;
    uint32_t entries_dispatched;
    uint32_t entries_refused;
    uint32_t drain_ticks;
    uint32_t stream_ticks;
    uint32_t voice_ticks;
    vf2_status drain_status;
    vf2_status stream_status;
    vf2_status voice_status;
} vf2_session_audio_pump_report;

/* Bridge one frame's worth of live sound: drain the i960 queue
 * (0x504001 ring, drainer 0x43800 semantics on a shadow CPU --
 * the session CPU is untouched) through the UART slot into the
 * board's L3 ring file, then dispatch each newly filed entry,
 * consuming refuse-doors the way the silicon drain epilogue does
 * (cursor advance regardless of outcome). Then advance the board
 * one frame of its own: the L2 stream tick and the L1 voice
 * maintenance (the recovered timer ISRs; on hardware they are
 * asynchronous, one tick per pumped frame is the model
 * convention). Only live-filed queue content flows; nothing is
 * staged or seeded. Host-driven (call after step, before
 * render_audio), never implicit in step -- pumping mutates
 * IO-control RAM and would otherwise perturb oracle digests.
 * Returns VF2_OK unless the session/audio is unusable (bad
 * argument, audio not attached). */
vf2_status vf2_session_pump_audio(
    vf2_session *session,
    vf2_session_audio_pump_report *report
);

/* One sound-drain interrupt (vector 15) service report (v0853).
 * `armed` is enable bit 10 of 0xe80004 at call entry; `ticks` counts
 * ISR services run (one per byte emitted plus the ROM's final empty
 * pass, which disarms); `bytes` holds what the ROM stored to the UART
 * data slot, in order; `entries_popped` is the queue count delta. */
enum {
    VF2_SESSION_SOUND_DRAIN_MAX_BYTES = 64,
    VF2_SESSION_SOUND_DRAIN_TICK_CAP = 80
};

typedef struct vf2_session_sound_drain_report {
    uint32_t armed;
    uint32_t ticks;
    uint32_t bytes_moved;
    uint32_t entries_popped;
    uint8_t bytes[VF2_SESSION_SOUND_DRAIN_MAX_BYTES];
} vf2_session_sound_drain_report;

/* Run the real ROM sound-drain ISR on this session's CPU until the
 * line disarms, reporting what the ROM sent (v0853).
 *
 * The i960 sound line is external interrupt vector 15: request bit 10
 * of 0xe80000, enable bit 10 of 0xe80004. The ROM arms it itself --
 * the queue filer 0x43888 pokes [0xe80004] with 0x21 then 0x421 on
 * accept -- and the handler 0xe10 (gated on enable bit 10) calls
 * 0x437d0, which acknowledges the line and drains exactly ONE byte
 * per service to the Model 2A UART data slot 0x01c80000, re-arming
 * while entries remain and returning without a re-arm when the queue
 * runs out. The frame loop injects only vector 12, so the oracle
 * never runs this ISR; no live UART write was ever observed in the
 * 8c/8f hunts because of it (and a hunt that watched 0x009c0000
 * instead of the ROM-staged 0x01c80000). This primitive services the
 * line the way the enabled device would: while armed, raise the
 * request bit and enter the vector, one full ISR invocation per
 * tick, reading the byte the ROM stored after each tick.
 *
 * Measured contract (out/park-sixth-a6c0.vf2snap, live 3-entry
 * queue, armed): 10 ticks, 9 bytes ae 10 1f 9e 0a 7f ae 10 1f, 3
 * ring entries popped, end state count 0 / rcur 3 / byte index 0xff
 * / staged word 0x00ae101f / enable 0x21 (the ROM's own empty pass
 * disarms) / request 0x00000000. The pump's shadow-CPU drain emits
 * the identical bytes and queue cells; the one measured difference
 * is the enable register, which the shadow drain leaves at 0x421
 * (it stops one tick earlier). See
 * decomp/i960/notes/audio_isr_v15_live_v0853.md.
 *
 * Host-driven, never implicit in step (the pump's contract): an
 * unarmed line reports VF2_OK with zero work, exactly the measured
 * negative. Fails closed -- VF2_ERROR_UNSUPPORTED -- on a session the
 * recovered native runtime last advanced (the drain runs raw
 * interpreter instructions on the session CPU, only sound on
 * interpreter-driven state) and on any tick that does not return to
 * the interrupted IP within the step/tick caps. */
vf2_status vf2_session_service_sound_drain(
    vf2_session *session,
    vf2_session_sound_drain_report *report
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
