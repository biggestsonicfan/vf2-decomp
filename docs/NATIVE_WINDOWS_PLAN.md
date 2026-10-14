# Plan: finish the recovery and run natively on Windows

Status: proposed (2026-10-13). Supersedes nothing: it sits on top of
`decomp/i960/notes/completion_plan_v0734.md` (oracle integrity, bookkeeping,
named admissions, simulation systems) and `docs/PATH_TO_TEN.md` (bar-by-bar
scope), and adds the pieces they do not cover: a Windows host, cold boot,
a decision on how to reach the unrecovered code, and the rendering and audio
back ends.

The rules in `AGENTS.md` and `docs/LEGAL.md` apply to every phase unchanged
(evidence before implementation, fail closed, the i960 reference execution is
the oracle, no ROM-derived artifacts in Git, work directly on `master`,
one coherent commit per slice). Where this plan needs a rule bent, it says so
as a decision gate (G1, G2) and stops there for the maintainer.

## Where we are (measured, 2026-10-13)

- Before the Phase 0 commits, `master` did not build under `-Werror` on the
  ubuntu-latest toolchains (about 50 diagnostics) and the sanitizer job found
  two real defects. After them, gcc 13, clang 18 and clang ASan/UBSan build and
  pass `ctest` without ROMs (110 tests, one skipped for lack of the gitignored
  trace corpus).
- The recovered native runtime only advances from a proven i960 snapshot
  (`--native-snapshot`). The synthetic orchestrator snapshot is refused
  ("unsupported operation"), so there is no cold boot to the title screen.
- No host window, no host audio, no gamepad: `src/platform/` is a software
  surface plus a null backend.
- Hand recovery is at about 83k lines of C for a 512 KiB program ROM; the
  simulation systems (hit boxes, damage, physics, AI) are not started.
- Audio: 68000 oracle (decoder + stepper) exists and its PC-relative base is
  fixed (PR #123). The sample directory is now located: 650 entries x 20 bytes
  at program offset `0x5b28` (see Phase 8).
- Geometry: the TGP FIFO protocol is captured from MAME
  (`tgp-interface-capture`), the coprocessor itself is not modelled.

## Principles specific to this plan

1. A phase is done only when its exit criteria are met by a command anyone can
   re-run; "looks right" never closes a phase.
2. No ROM, no snapshot, no trace and no generated C from the ROM goes into
   Git. Hashes and counters are fine.
3. Native means native: the shipped Windows build runs recovered or
   recompiled C, not an i960, 68000 or SCSP interpreter. Interpreters stay as
   test oracles and as scaffolding that is counted and shrinks every phase.
4. Every scaffold is tagged `HLE-TEMP` in the source and listed in
   `docs/SCAFFOLDING.md` (created in Phase 1) with the phase that removes it.

---

## Phase 0 - Green gates (done, pending push)

- Fix the `-Werror` breaks on gcc 13 / clang 18, the two sanitizer findings
  and the ROM-less skip of `p1_real_trace_demo`.
- Add a `windows-latest` MSVC job (configure, build, `ctest`) so the Windows
  build cannot rot while the host is written. It starts non-blocking
  (`continue-on-error`) because it has not yet been observed green; it becomes
  blocking once it is.
- Add `scripts/rom_gate.sh` / `scripts/rom_gate.ps1`: configure with
  `-DVF2_ROM_DIR`, run the ROM-backed tests with `-j2` (the heavy ones use a
  lot of memory) and print a one-line summary to paste in the commit message.

Exit: CI green on the three Linux jobs; the Windows job observed green once
and then made blocking; the ROM gate passes on the maintainer's machine.

## Phase 1 - Windows host shell with SDL3

Status: implemented (`src/host/`, `include/vf2/host.h`, `include/vf2/session.h`,
`VF2_BUILD_HOST`, CI job `host`, `docs/SCAFFOLDING.md`). Verified locally on
Linux with the dummy drivers; the real-snapshot run and the Windows CI job are
still to be observed. The out-of-tree prototype (item 5) is not contributed yet.

Goal: a `vf2_host` program that opens a window, shows the frame, takes
keyboard and gamepad input and opens an audio device, using the existing game
core, on Windows first and Linux for CI.

Why SDL3: one dependency covers window, input, gamepads, audio, timing and a
GPU API (`SDL_GPU`, which sits on Vulkan, Direct3D 12 and Metal), so Vulkan can
be added in Phase 7 without writing a second host. The core never includes SDL
headers; `src/host/` is the only code that does.

Work

1. `src/host/host_sdl.c`: window, 60 Hz pacing, integer and aspect scaling,
   fullscreen, `SDL_AUDIO` stream fed from a core-side ring (silence until
   Phase 8), keyboard and gamepad mapping to `VF2_PLATFORM_BUTTON_*`
   (remappable through a small ini file), pause and frame-step for debugging.
2. Core-side seam: `include/vf2/host.h` with `vf2_host_frame` (pixels, size),
   `vf2_host_audio` (stereo S16 at 44100 Hz) and `vf2_host_input` (bit mask).
   The existing `vf2_platform_*` software surface becomes the first producer.
3. CMake: `VF2_BUILD_HOST` option, SDL3 pinned (FetchContent, or vcpkg on
   Windows), static link, no ROM or asset embedded.
4. CI: build `vf2_host` on ubuntu and windows-latest, run it with
   `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` for N frames, publish a zip
   with the exe (no ROM) as an artifact.
5. Out-of-tree prototype to reuse: Win32 GDI window, waveOut, XInput and a
   native 32-voice PCM mixer (16-bit big-endian samples read straight from the
   sample ROM) exist and will be contributed as reference code.

Exit

- `vf2_host --rom-dir D --snapshot S` shows the snapshot's frame, reacts to
  keys and pad (a test reads the game's input ring after injected SDL events),
  and exits cleanly; the dummy-driver run is a CTest entry.
- Windows artifact builds in CI.

## Phase 2 - Cold boot (no snapshot)

Status (2026-10-13), step 1 measured:

- From reset the oracle reaches the first frame-wait loop at `0x0004afdc`
  after 2,985,240 instructions, and then spins there because nothing injects
  the frame IRQ (it needs the host clock).
- `vf2i960 native-sixth-dispatch <roms> <out.vf2snap>` goes from reset to the
  sixth dispatch with `Final CPU and memory state: MATCH` in about 6 s
  (7,404,901 repeated-frame instructions compared). `scripts/cold_snapshot.*`
  wraps it, so a start state needs only the ROM.
- `vf2 --native-snapshot` (and any session) refuses that state, and every other
  state, with "unsupported operation" because `vf2_sound_board_initialize`
  fails closed on the real audio ROM: the boot upload row at `0x608df0` is
  `0x00010060`, below the 68000 ROM window, so the helper returns
  UNSUPPORTED (`src/hardware/sound_board.c`, row read). It also fails at
  `6a57654d`, before the PC-relative fix. The host now treats that as "run
  silent" and reports it; video no longer depends on it.
- From that state 300 frames run with no error, but they draw nothing
  (`triangles=0`, black frame): the corridor is boot, not yet the attract loop.
  Steps 2 and 3 below are what remains.
- Reference (MAME, same ROM, from reset): the attract mode draws 3D scenes by
  frame 300 (character on a courtyard, "INSERT COIN(S)", "CREDIT 0/2"), so the
  black native frame is a real gap, not the expected boot screen. `0x508000`
  is rewritten every frame from frame ~25 (`0x8a00`) and becomes `0x18a00` at
  frame ~800. `base+0x3350/0x3351` stay 0 during the whole attract, so they
  are not a progress indicator.
- The determinism half of the exit criterion was met earlier:
  `scripts/frame_gate.sh <roms>` runs 3,600 frames from the cold snapshot with
  no error and identical work-RAM digests every 60 frames across two runs
  (`vf2_host --digest 60 --fast`).
- The native-versus-oracle half is now met. `vf2_host --oracle`
  (`vf2_session_step_oracle`) advances each frame through the pure i960
  interpreter from the same cold snapshot, with the frame interrupt driven by
  the same host-side scheduler the native path uses.
  `scripts/frame_oracle_gate.*` runs 3,600 oracle frames against the recovered
  runtime and requires byte-identical work-RAM digests at every jointly
  sampled frame: measured all 3,600 equal (the native runtime injects a vblank
  at both the early- and main-wait entry per visual frame, so it emits two
  digests per oracle digest and the states coincide at the main wait, i.e.
  oracle frame k == native frame 2k-1; the gate checks that for every frame).
- The interpreted-instruction counter is in place. `vf2_i960_cpu` now carries
  `interpreted_instructions` (the subset of `executed_instructions` the
  interpreter decoded), `vf2_session_instruction_counts` reports it relative to
  the start snapshot, and `vf2_host` prints a status line
  (`interp mode ... interpreted N recovered M`). Measured: the cold attract
  corridor runs with `interpreted 0` for 3,600 frames -- it is fully recovered
  C, which is the Phase 4 baseline. The oracle run reports `recovered 0`
  (100% interpreted), confirming the split.
- Still open: a picture. The native frame is still black (`triangles=0`); the
  attract 3D scenes need the geometry pipeline (Phase 6 / G2), so the visual
  gap is tracked there, not here. The work-RAM digest criterion above does not
  depend on it.

Goal: `vf2_host --rom-dir D` runs from the reset vector to the attract loop.

Work

1. Find out exactly where the cold path stops today (instruction address,
   missing device behaviour, `VF2_ERROR_UNSUPPORTED` site) with
   `vf2cycles --boundary-probe` from reset, and write it up.
2. Close those sites one at a time with the usual evidence and a differential
   against the executor oracle. Every boundary crossed through the interpreter
   is counted (`interpreted_instructions` in the status line).
3. Frame loop: video frame IRQ, timers and input ring driven by the host clock.

Exit

- 3,600 frames from reset with zero `VF2_ERROR_UNSUPPORTED`, deterministic:
  the per-frame digest of work RAM matches between two runs and between the
  native runtime and the oracle at every 60th frame.
- The interpreted-instruction counter is reported and is the baseline for
  Phase 4.

## Phase 3 - Reference sessions and coverage map

Status (2026-10-13): core tooling in place; one session reachable so far.

- `vf2i960 coverage <roms> <snap> <frames> <out.json> [frame:mask,...]` runs the
  interpreter oracle from the cold snapshot for N frames (same frame-interrupt
  scheduler as Phase 2) and writes a compact per-word hit-count JSON. Only
  counts leave the tool; it is deterministic (identical JSON across two runs).
- `tools/python/coverage_report.py` maps one or more session JSONs onto
  `decomp/i960/functions.csv` and prints, per session and for the union:
  recovered vs unrecovered vs lifted executed-word percentages (lifted is 0
  until the G1/Phase 4 decision), the count of recovered functions exercised,
  and the never-executed set. `scripts/coverage_report.*` reproduces the whole
  thing from the ROM and refreshes the committed backlog.
- Measured, attract session, 3,600 frames: 899 distinct program words executed;
  72.1% are inside a recovered function, 27.9% (251 words, 85 runs) are not and
  form the ranked backlog (`decomp/i960/coverage_attract_backlog.csv`); 61/111
  recovered functions are exercised. The hottest unrecovered runs are the
  scheduler/frame dispatch around `0x00010d54..0x00010e60` and `0x0000a15c`.
- Bookkeeping caveat (the plan's "fix the debt first"): the native hybrid runs
  this corridor with `interpreted 0`, so the 27.9% "unrecovered" is executed
  code the runtime recovers as dispatch/bridge blocks but that is not yet
  *registered* as a function in `functions.csv`. The backlog therefore mixes
  genuinely-unrecovered code with registry debt; reconciling `functions.csv`
  against the native dispatch is the next step before the numbers are fully
  trustworthy.
- Still open (deferred): the coin, character-select, 2-round match, results,
  continue and test-menu sessions. The cold corridor is still the black-frame
  attract spin and does not yet poll coin (needs the geometry pipeline, Phase 6,
  or a MAME-captured input stream), so scripted input currently changes nothing;
  the `[frame:mask]` argument is in place for when the game advances. The caller
  graph and the MAME cross-capture are also deferred to that point.

Goal: know, with numbers, how much of the program a real session executes and
which functions are still unrecovered.

Work

1. Record reference sessions with scripted inputs: attract, coin, character
   select, a full 2-round match, results, continue, test menu. Use MAME for
   independent hardware behaviour and the in-repo oracle for exact CPU state;
   keep only digests and counts in Git.
2. Build the coverage map: executed 4-byte words per function, hit counts,
   caller graph, per-session and union. Fix the known bookkeeping debt first
   (container rows, `completion_plan_v0734.md` 1.2-1.4) so the numbers can be
   trusted.
3. Rank unrecovered functions by executed frequency and by blocking effect
   (what stops a session from advancing).

Exit: `tools/python/coverage_report.py` prints recovered / lifted /
interpreted / never-executed percentages for each session and the union,
reproducibly, and the ranked backlog is committed (names and addresses only).

## Phase 4 - Reaching the rest of the program (decision gate G1)

Hand recovery of the simulation systems is months of work. A second route
exists and should be decided explicitly, not drifted into.

G1 - DECIDED 2026-10-14: the maintainer (celsowm) chose **Option B**. The
design and running record live in `docs/VF2LIFT.md`; slice 1 (the
`lifted_instructions` counter and the `vf2_i960_step_lifted` primitive) is
committed. The "plays a full match" half of the exit below is gated on G2
(geometry) and Phase 7 (video) exactly as the Phase 3 remainder is, so vf2lift
is built and validated on the reachable attract corridor first.

G1 - maintainer decision. Option B: a build-time recompiler, i.e. a tool that
reads the user's own ROM, lifts every function reachable from the CFG to C
using the same instruction semantics as `src/i960/executor.c`, and writes the C
into the build directory only. Nothing generated is ever committed (the same
position as the interpreter, which also reads the ROM at run time). Hand
recovered functions override lifted ones through a registry, so the readable
clean-room code stays the canonical product and the lifted code is a bridge
that makes the game run natively while recovery continues. It is opt-in
(`VF2_ENABLE_LIFTED`, default OFF) and labelled as scaffolding. Option A: stay
with hand recovery only and accept a much longer path to a playable build.

If B is accepted

1. `tools/vf2lift`: CFG-driven i960 to C, one C function per guest function,
   registers and condition codes as explicit state, call/return through the
   existing frame model, indirect jumps resolved from the recovered jump tables
   or routed through a checked dispatcher that fails closed.
2. Per-function differential: run the lifted function and the executor from the
   same captured state (from Phase 3 sessions), compare registers, condition
   state, frames and every memory write. A function that does not match stays
   unlifted.
3. Registry: `recovered > lifted > interpreted`; the status line shows the
   split.

Exit (B): every function executed in the Phase 3 union is lifted or recovered;
interpreted instructions in a full session are zero; the lifted build plays a
session from reset to the end of a match and its per-frame work-RAM digests
equal the oracle's for the scripted inputs.

Exit (A): the Phase 3 backlog is ordered and the existing
`completion_plan_v0734.md` phases 3-4 continue, now measured against the
coverage map.

Status (B, 2026-10-14): the reachable half is done -- `tools/vf2lift` exists,
emits one C function per guest function reusing the executor semantics
(`vf2_i960_step_lifted`), is driven by the committed Phase 3 coverage backlog,
and is gated per function by a differential against the interpreter; the
`recovered > lifted > interpreter` dispatch core (`vf2_lift_dispatch_step`) and
the `executed`/`interpreted`/`lifted` split are in place; opt-in
`VF2_ENABLE_LIFTED` (default OFF). 16 non-overlapping attract-corridor functions
lift and match the oracle byte-for-byte with `interpreted == 0`. The remaining
half of the exit -- a lifted build that plays reset-to-match with matching
per-frame digests, and the three-tier wiring into the shipped session loop -- is
gated on G2 (geometry) and Phase 7 (video), since the attract corridor cannot
advance to a match without them. Detail and the slice log: `docs/VF2LIFT.md`.

## Phase 5 - Game flow and simulation

Goal: a full match runs natively with correct logic.

- Under B: validate and then replace lifted code by readable recovered C in
  coverage order (hit boxes, collision, damage, ring-out, physics, CPU
  decisions), each with its own differential, keeping the lifted version as
  the oracle until the swap.
- Under A: the same list, with no lifted bridge to run the rest meanwhile.
- Either way the match state machine (title, select, arena, fight, results,
  continue) is recovered from the Phase 3 sessions.

Exit: scripted two-round match (inputs recorded in Phase 3) ends with the same
winner, round timer and score as the reference; per-frame digests match.

## Phase 6 - Geometry (decision gate G2)

The TGP is a separate coprocessor; the i960 sends it a command stream and
reads transformed results.

G2 - choose the route. Option 1: implement the geometry pipeline natively from
the observed FIFO protocol (matrix, viewport, clip, lighting, depth as
measured). Option 2: lift the coprocessor program the same way as Phase 4,
since its instruction set is public in other emulators' sources and the program
is small. Pick by measured fidelity on the captured streams.

Work: decode every command class seen in the Phase 3 sessions (class =
`(word >> 23) & 0x1f` in the captured headers), write the native pipeline, and
compare its output with the reference for each captured frame.

Exit: for 100 captured frames spanning menus and fights, the native pipeline's
triangle lists equal the reference lists (count, vertices to 1/16 pixel,
texture and colour ids).

G2 - DECIDED 2026-10-14: Option 1 (native-from-FIFO) was chosen, then
reconsidered to **Option 2 (lift the TGP microcode)** after the evidence below.

Status (2026-10-14): **blocked — the geometry coprocessor is never exercised in
any reachable state, so neither option has evidence or microcode to build
from.** Verified four independent ways:

1. The `copro_data` ROM region is empty (no files map to it in the romset
   manifest; 0 nonzero bytes). Model 2A uploads the TGP program from host data
   at boot, so the microcode only exists in a running machine, never as a ROM.
2. Pure i960 interpreter from reset, 30M steps, halts spinning at `0x0004aff8`
   with `geometry_program_count == 0` — the reachable boot never uploads a TGP
   program.
3. The recovered native runtime to the sixth-dispatch checkpoint (the cold
   snapshot) has `geometry_program_count == 0` and a zero program buffer.
4. Frame-advancing the post-boot snapshot for 1200 frames while injecting
   coin + start returns the CPU to the vblank handler (`0x00000bc0`) every
   frame with `geometry_program_count == 0`: the attract loop does not advance
   and the TGP is never uploaded regardless of input (it does not poll coin).

Consistent with the recovery notes (`matrix_ports_p2_v0377.md` `absent`;
`tgp_w2_consumption_v0378.md` "unproven — this is the gap"): the reachable
i960 bus carries only ring bookkeeping and copro arithmetic, never a
matrix/focus/vertex stream.

Phase 6 step 1 done (2026-10-14): `vf2i960 geometry-census` (and
`scripts/geometry_census.*`) observes every 32-bit write into the geometry ring
(`0x800000`), the copro port (`0x880000`) and the geometry buffer RAM
(`0x900000`) over N attract frames and classifies by
`class = (word >> 23) & 0x1f`, reporting per class the count, distinct words,
known-protocol-immediate matches and sample headers. Over 600 attract frames
the entire stream is: class 0x00/0x01 ring status and buffer-RAM pointers;
class 0x04 (`0x12002424`) and 0x16 (`0x0b001616`) copro arithmetic tags (100%
protocol); class 0x06 (`223.2f`/`172.8f`) and 0x08 (`600.0f`) camera float
operands. There are **no renderable TGP geometry opcodes** (no object-with-
polygon-pointer, matrix, or vertex class) — the attract corridor computes
camera scalars on the copro FPU and never submits a mesh, so there is nothing
for a pipeline to turn into triangles until the game advances to a 3D scene.
This grounds the plan's "decode every command class seen" in-tree and
reproducibly; it is not the pipeline, which stays blocked as above.

**Prerequisite for Phase 6 (either option):** advance the i960 past the attract
loop into the self-running 3D demo or into gameplay, so the game uploads the TGP
program and submits real geometry. That is Phase 5 / deep i960 recovery
(understanding what the attract state machine waits on), or an external
reference (a MAME Model 2 run) to both supply TGP behaviour and drive the game
to a 3D frame. Until then Phase 6, Phase 5's match validation, the Phase 3
coin/select/match sessions and Phase 4's full-session exit all share this one
blocker.

Attract->3D interrupt analysis (2026-10-14, against a MAME reference;
`decomp/i960/notes/attract_interrupt_injection_vs_mame.md`): MAME fires, besides
vector 12 (VsyncScr), vector 14 (timer @0xd50, sets `0x50008c`) and vector 15
(sound @0xe10, drains the sound ring `0x504020` to serial `0x01c80000`) ~twice
per 2.4 frames during the attract wait, and transitions sound(725)->loading(727)
->3D(967). The native injects only vector 12. Injecting 14/15 at that cadence
(natural boot and from the cold snapshot) runs them correctly (vector 14 sets
`0x50008c` bit 0) but does NOT advance the attract over 1100+ frames -- so 14/15
are necessary but not sufficient. Leading hypothesis: the loading phase is gated
on the sound-board consuming/acking the command stream, which the oracle sound
model does not do. Pinning it needs a MAME RAM-state capture near frame 727
(`0x500704`/`0x5000a6`/`0x50008c` and the loading/ready flag the loop polls).

## Phase 7 - Video and the Vulkan/SDL_GPU renderer

Work

1. Tile layers, palette, texture memory and polygon submission in a software
   reference renderer (extends `platform_software.c`), 496 x 384.
2. `SDL_GPU` renderer (Vulkan on Windows and Linux, D3D12 option) producing the
   same image: textured, Z-buffered polygons, tile layers, internal-resolution
   scaling, optional CRT filter.
3. Image-diff harness: reference screenshots (kept outside Git) compared with
   the software renderer and then the GPU renderer, tolerance documented.

Exit: attract mode, character select and a match render recognisably;
software vs GPU differ by less than the documented tolerance on 100 frames;
60 fps at 4x internal resolution on an integrated GPU.

## Phase 8 - Audio, native end to end

The sound hardware is a 68000 running a driver, an SCSP and a sample ROM. The
shipped path stays native: the driver's logic recoded in C, driving a native
mixer (no SCSP register emulation, no 68000 interpreter).

Measured so far: commands are MIDI-style bytes (`0x80` note off, `0x90` note
on, `0xA0`, `0xB0`, `0xC0` program change, `0xE0` pitch bend) in a ring at
`0x1000`; a program selects a 6-byte-per-note table (pointer tables at
`0x6032c6` / `0x6053aa`); each note names an entry of the sample directory
at program offset `0x5b28` (650 entries x 20 bytes: start address, length in
bytes, loop start, loop length, envelope word), mirrored to sound RAM `0x5000`
at boot. Verified byte for byte against MAME's sound RAM. Not yet mapped:
pitch (octave and fine tune from the note and the tuning word) and the
envelope and volume path.

Work: map pitch, volume, pan, envelope; recode the note-on, note-off,
controller, program-change and pitch-bend handlers and the voice-maintenance
ISR in C (the 68000 stepper is the oracle; voice scan with `d2` as an explicit
input); drive the mixer; capture key-on events from MAME for scripted command
sequences and compare (sample, pitch, loop, level).

Exit: per command class, the C driver and the 68000 oracle produce the same
voice parameters on the captured sequences; the attract music and the fight
effects play through the host; the two `WILL_FAIL` m68k tests are removed
because they pass.

## Phase 9 - Playable release

Work: coin and credit handling, NVRAM and EEPROM persistence, test and service
menus, two-player and gamepad configuration screen, settings file, frame
pacing and performance, crash-safe fail-closed messages ("unsupported at
0xADDR" shown instead of silent misbehaviour), packaging (zip, optional
installer), regression pack of digests and image diffs run in CI.

Exit (v0.1.0): a person with their own ROM set runs `vf2_host.exe`, plays a
complete credit (select, two rounds, results, continue) with sound and
pad, on Windows 10/11, with zero interpreted instructions, and every
`HLE-TEMP` item either removed or listed as a known limitation.

---

## Dependencies and parallel work

```text
P0 -> P1 -> P2 -> P3 -> P4(G1) -> P5 -> P9
          \                      \
           P7 (needs P6)          P8 (needs P1 for the audio device)
P3 -> P6(G2) -> P7 -> P9
```

Four people or agents can work at once without touching the same files:
host (P1, P7), boot and coverage (P2, P3), logic (P4, P5), hardware
(P6, P8). Rebase on `master` daily; announce which files a slice touches.

## Standing checks for every slice

1. `cmake --build` and `ctest` on gcc, clang, ASan/UBSan and MSVC.
2. The most specific ROM-backed differential for the change, run with
   `scripts/rom_gate.*`; the commit message states what ran.
3. `git diff --stat` shows no ROM, snapshot, trace or generated C.
4. The scaffolding list and the coverage report are updated.

## Risks

- Cold boot may need hardware behaviour that was never observed (Phase 2);
  mitigation: capture it from MAME before guessing.
- G1 option B must not erode the clean-room goal; mitigation: opt-in,
  uncommitted output, readable code remains canonical, counters public.
- Rasterizer detail (fill rules, texture filtering) can only be judged against
  reference screenshots; keep a small set outside Git and list the hashes.
- Heavy ROM-backed tests are memory hungry; run them at `-j2`.

## Instruction for an agent given this plan

Work through the phases in order. For each phase: implement, run the standing
checks, commit to `master`, update this file's status line, then continue. Stop
and ask only at G1 and G2, or when a check fails for a reason that is not
understood. Report the final commit SHA and the exact validation run after
every phase.
