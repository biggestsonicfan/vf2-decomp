# T5 / 8/10 close-out: live packets flow in-session (v0825)

Status: 8/10 CLAIMED with the scope below. Trigger plan T1-T4
complete; bridge (T3) + hunt (T4) + differentials (T2/8h) green
on 186/186. This note is the claim record.

## What 8/10 means (and does not)

8/10 = LIVE-FILED sound packets flow through the session with
every link proven. Not audible gameplay: no live voice renders
in-session yet (engine keying is oracle-side), and no gameplay
context files anything in 70+ hunted frames.

## Evidence per link (all on master)

- Writers: 80 `call 0x43888` sites in 16 clusters (T1 byte
  census). Boot chain caught live (T4): scheduler task
  dispatch `callx` -> flusher 0x43abc (frame 17) ->
  `0x58eac` immediate filer files 0x00ae101f (frame 18,
  count 2 -> 3 = the park state).
- Queue -> bytes: reference drainer 9 ticks emit
  ae 10 1f | 9e 0a 7f | ae 10 1f, sentinel cycle (T2 probe).
- Bytes -> ring: 9 L3 file legs + 3-entry live ring FULL
  MATCH C-vs-stepper, exact packet bytes (T2/8g).
- Ring -> voices: round-1 90-class FULL MATCH at the 0xc68
  engine door (S4 arming); the live {9e} command authors
  voice0 (+2 = 0x58); determinism holds (T2). a0 rounds
  refuse BY DESIGN at the 0x1e74 entry with clean 249-step
  S drain exits (residue-history corroborated).
- Session: `vf2_session_pump_audio` (shadow-CPU drain,
  silicon-consume dispatch, host-driven/digest-safe) wired
  into host_sdl; pump ctest from the live queue pins 9
  bytes / 3 filed / 0 dispatched / 3 refused on the
  init-state board (correct: no staging), determinism
  holds (T3).
- Audibility of the MECHANISM: synthetic chain renders
  peak 13964, deterministic, WAV (8h). Live chain renders
  peak 0 (correct: stops + no keying).

## Explicitly open (9/10 and beyond)

- Gameplay writers: PUNCH/KICK/START-edge sixth-fresh,
  fighter-task frame all negative (70+ frames); contact
  snapshots don't exist; entry-2 (9e0a7f) filer uncaught.
- Engine (0xc68+) keying/voicing: arrivals name the door,
  outcomes stay oracle-side. a0 body: residue-dependent,
  unrecovered by measurement (not by neglect).
- Menu-lever replay from a pre-gating park (25 idle legs).
- L1/L2 ticks in the host loop (game_update guard).
  (v0850 audited the fail-closed pins: 13 planted fail-open mutations,
  8 already pinned, 4 test holes closed with new C-level pins, the
  pump's unattached guard documented unpinnable; see
  `audio_failclosed_pins_audit_v0850.md`.)
  (v0852 closed the item: the L2 stream tick is recovered (0x1f16
  head pokes + prescaler/worker countdown decrements; armed records
  refuse cleanly) and `vf2_session_pump_audio` runs one L1 + one L2
  tick per pumped frame, reported as `stream_ticks`/`voice_ticks` +
  statuses; `game_update`'s oracle guard stays as designed. See
  `audio_l2_stream_tick_v0852.md`.)
- Full SCSP DSP / TGP / rasterizer: out of scope per
  README principles, independent of this bar. (v0826 later measured
  the DSP program + effect-return configuration; execution/audio stays
  out of scope.)

## Methodological correction to UNCOVERED_BRANCHES §5

" No 68k execution oracle exists" is stale: the m68k stepper
(`src/m68k/stepper.c`, `vf2_m68k_run`) IS the sound-CPU
oracle, and every 8g/T2/8h leg is a C-vs-stepper
differential (full-RAM + regs + SCSP + step pins), not
C-vs-C. What remains C-vs-C is only the never-stepped
remainder (engine past 0xc68, a0 body effects).
