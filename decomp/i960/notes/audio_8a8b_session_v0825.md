# 8a/8b: producer hunt + session attach (v0825)

Status: producer hunt closed as UNMAPPED (honest negative);
session attach proven on real banks (attach OK, render
deterministic silence). Sound gate green. The dispatch bridge
(main-CPU -> sound-board packets in-session) is the remaining
8/10 work; 8b deliberately does not fake it.

## 8a: producer hunt (no code change, read-only)

Question: who emits sound packets in-session?
Answer: NOBODY. Three independent evidences:

1. `emit_command` / `dispatch_next` have ZERO callers outside
   tests (grep over src/: only game.c's render path touches the
   board, and it only renders).
2. `model2a.c` models NO sound region: map covers main ROM /
   data / geometry / copro / work RAM / video / timers / tiles /
   palette / IO / SRAM / textures / luma / system -- no audio
   port, no shared command window. The main CPU has nowhere to
   write a sound packet.
3. `game_run_native_frame` never touches `game->sound`; the host
   loop (host_sdl.c main) DOES call `session_render_audio` per
   frame and pushes to the ring -- the bridge exists downstream
   but feeds init-state silence.

So session audio today = attach OK + render zeros. The S1-era
"feeder" answer (0x90 HIGH fall-through) is about the m68k
drain internals, not the session producer. Do not seed packets
in-session to fake 8/10; that would be test staging laundered
as integration.

## 8b: session attach (test_session_attach, ROM-backed)

Real AUDIOCPU (0x80000) + SAMPLES (0x800000) regions via
`vf2_romset_build_region` (the session's own builder), then the
session's exact calls: `game_initialize` + `game_attach_audio`.
Gates: attach VF2_OK, 735-frame render twice OK +
bit-identical. Peak 0 reported (expected, per 8a).

Kills the stale session.c claim ("boot still refuses the real
audio ROM... Phase 8 removes this"): init has been real-ROM
clean since S4a; the comment predated V2's true-semantics
uploader fix.

## Correction: S4b bank order

S4b concatenated mpr-17570..73 in NUMERIC order. The real
SAMPLES region (rom_manifest.c:40-43) is 73@0, 72@2MB, 71@4MB,
70@6MB -- numeric order is BACKWARDS. Only timbre was at stake
(S4b claims audibility/control, not timbre), but every future
window must use `build_region(SAMPLES)` or the 73-first order.
The 8b leg uses the builder, so it is order-correct by
construction.
",
