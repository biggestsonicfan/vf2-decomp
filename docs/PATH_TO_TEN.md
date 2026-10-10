# Path to all bars at 10/10

This is a planning note (no code change). It maps the README
progress bars to specific work items, identifies the
dependencies between bars, and gives an honest scope
assessment for reaching 10/10 on every row.

The bars are **qualitative** (per the README itself) — they
are not a percentage of decompiled code, but a view of
progress toward running the **complete game natively**.

## Current state (post-v0773, master = 93e13e92)

| Bar | Now | To 10 |
|---|---|---|
| ROM / boot | 10 | (done) |
| Model 2A hardware | 8 | TGP program execution, polygon rasterization, full SCSP/PCM |
| scheduler / runtime | 10 | (done) |
| input | 8 | host-side platform adapter |
| camera | 8 | full combat camera path |
| HUD / game_disp | 9 | F1 starting state, in-fight HUD, results screen |
| fighter / game logic | 8 | combat, physics, hitboxes, damage, AI, ring-out |
| geometry / rendering | 7 | TGP firmware execution, polygon rasterization, framebuffer |
| audio | 5 | SCSP FM/DSP emulation, ADSR envelope, full PCM streaming |
| complete game flow | 5 | match state machine (start fight / fight / end / results) |
| fully playable match | 2 | needs every other bar ≥ 9/10 |

## Bar-by-bar definition of "10/10"

### ROM / boot — done (10/10)

Validated 36/36 ROMs, reset-vector parser, IAC reinitialize
copy, boot_entry, boot_stage_2. Nothing to add.

### Model 2A hardware — 8/10 → 10/10

The current Model 2A boundary covers the **common bus** that
both the i960 program and the TGP coprocessor touch:
one-shot 25 MHz timers, IRQ vector/request/acknowledge,
video frame status, render-mode control, TGP
program-upload accounting.

For 10/10 the boundary must additionally model:
- **TGP program execution** (the coprocessor's microcoded
  geometry pipeline that consumes the uploaded program).
  This is a separate CPU; reverse-engineering its
  microcode is a multi-year slice on its own.
- **Polygon rasterization** — fill rules, depth/Z buffer,
  texture sampling. Currently the project submits triangles
  to a host-side backend.
- **Full SCSP backend** — see the audio bar.
- **Full video timing** — vsync, hsync, scanline buffers,
  tile RAM, palette RAM. Currently headless.

### scheduler / runtime — done (10/10)

1,270,824+ recovered instructions, zero interpreted, full
per-block differential through the sixth dispatch. Already
at 10/10.

### input — 8/10 → 10/10

The game-facing input path is recovered (P1/P2 input
sequence gates, input ring buffer, frame_input_update,
0x14288/0x142c0 setup, observed state-4/state-8 matrix).
What remains is the **host-side platform adapter**:
- keyboard/joystick → game-internal input bit translation
  (P1/P2 button-by-button, D-pad, service/coin);
- gamepad polling/edge detection on the host;
- input injection into the model2a via `vf2_model2a_set_input`.

This is a clean, bounded slice: read the existing i960
input protocol and reproduce it in C with a SDL2 (or
null) backend.

### camera — 8/10 → 10/10

The post-fight camera (`camera_post_update_gate`,
`camera_apply`, `camera_first_update`,
`camera_fill_viewport_table`, `camera_range_window`,
`camera_classify_range`,
`camera_project_fighter_ranges`) is recovered. The
in-fight camera (per-frame tracking shots, zoom,
cut-away) is **not** — the in-match camera path is gated
by the unrecovered match state machine, so it has not
been traced from a real fight snapshot.

For 10/10: trace the in-fight camera dispatcher(s) from a
real match snapshot, identify the per-frame calls, recover
the camera-object dispatchers and project logic. This is
a slice-3-class task (~1000 B of recovered C).

### HUD / game_disp — 9/10 → 10/10

The TEST MENU / COIN ASSIGNMENT corridor (rows 1-4 in
COMMON and INDIVIDUAL modes) is fully recovered. F2/F3/F4
post-edit release is FULL MATCH. What's left for 10/10:
- F1 starting state (cursor dispatch at `0x59f34` +
  row-4 handler at `0x5a0a4` per
  `f1_manual_setting_entry_boundary_v0730.md`).
- In-fight HUD: timer, health bars, round counter.
  These are drawn by the post-0x28178 object dispatcher
  chain; the cells are partially recovered but the
  timer/hp-bars/round-counter rendering is not.
- Results screen: KO display, "WIN/LOSE" text, continue
  prompt.

The F1 slice is well-bounded (per
`f1_manual_setting_entry_boundary_v0730.md`). The
in-fight HUD is in the wider object dispatch chain and
needs the same recovery cadence.

### fighter / game logic — 8/10 → 10/10

This is the **headline** remaining area. The
`phase_4_simulation_boundary_v0738.md` note maps it. The
key gaps:
- **Physics / idle-to-crouch transition** — the
  `0x14288` walker is recovered; the per-state
  transition is in an untraced sub-callee.
- **Hitbox / hurtbox writes** at `+0x61e` / `+0x626`
  in work-RAM — the hitbox function is inside the
  `fa_player_1442c` walker; not yet identified.
- **Damage application** — who computes hit damage and
  where; the chain goes through `0x4d2c0..0x4e808`
  (gap run, partly covered by v0771 hash sub-chain).
- **Ring-out detection** — when a fighter is pushed off
  the edge; gated by arena geometry.
- **CPU AI** — the 1P "COM" opponent decision logic; not
  yet traced from a real match snapshot.

For 10/10: ~5,000+ lines of recovered C across 10-20
sub-leaves. Tractable per the existing v0741-v0773 cadence
(40-108 B per leaf, 1-2 hours each), but the total scope
is months.

### geometry / rendering — 7/10 → 10/10

The TGP upload protocol is captured (per `STATUS.md`:
"recovered; stateful direct/object geometry reference
executor with matrix/viewport/depth submission added").
The polygon command packing and ring commit are recovered
for the observed four-entry boundary.

What remains:
- **TGP firmware execution** — same as in
  Model 2A hardware; the TGP is a coprocessor with
  microcode.
- **Polygon rasterization** — fill rules, depth testing,
  texture sampling. Not recoverable from the i960
  program; needs the TGP microcode reverse-engineered.
- **Tile / palette timing** — vsync, scanline buffers.
- **Framebuffer present** — the headless software surface
  works, but a windowed display adapter is needed for
  actual play.

This bar is **architecturally coupled** to the
`Model 2A hardware` bar: TGP execution is hardware, not
game logic. The two share the same unknown.

### audio — 5/10 → 10/10

The 68000/SCSP/PCM path is **partly** recovered (per
`ROADMAP.md` v0.4.0): ROM vector table, Model 2 sound
board map, instruction-aligned shared-interrupt and
32-entry voice-maintenance state transition,
producer/consumer command ring with bounded 0xc0,
no-live-voice 0xe0, selected 0xb0 handlers, deterministic
PCM slot renderer with sample-address progression,
looping, stereo pan and slot ADSR lifecycle.

What remains:
- **SCSP FM synthesis** — 4-operator FM per voice.
  Reverse-engineering the YMF262 (or its Model-2 variant)
  is a separate research project.
- **SCSP DSP effects** — the Model 2 SCSP has a DSP for
  reverb/chorus. Unrecovered.
- **Full command decoding** — many 0xb0 subcommands are
  still "open" per `ROADMAP.md`.
- **Hardware-accurate envelope timing** — currently
  deterministic, not cycle-accurate.

For 10/10: another 6+ month slice of reverse engineering
the SCSP YMF262 + DSP + envelope. There is no short path;
this is a self-contained piece of Yamaha silicon
documentation work.

### complete game flow — 5/10 → 10/10

The TEST MENU / COIN ASSIGNMENT corridor (rows 1-4) is
recovered. Boot/menu/phase corridors work. What's left:
- The **match state machine** itself: title → main menu →
  character select → arena select → fight → results →
  continue/rematch → back to main menu.
- The transitions between these states are gated by the
  game-flow dispatcher(s) and have not been traced from a
  full match snapshot.

For 10/10: trace the in-game state transitions from a
real "title-to-fight" snapshot, identify the
game-flow-dispatch call chain, recover 2-3 medium-sized
state-machine dispatchers (~200-400 B each).

### fully playable match — 2/10 → 10/10

This is the headline. It **requires** every other bar at
≥ 9/10. It cannot be moved by a single recovery; it
requires:
- input bar at 10/10 (so the player can press buttons)
- fighter / game logic at 10/10 (so the fighter
  responds)
- geometry / rendering at 10/10 (so the player sees it)
- audio at 10/10 (so the player hears it)
- complete game flow at 10/10 (so the match starts and
  ends cleanly)

The two hardest are `fighter / game logic` and
`geometry / rendering` (which couple to TGP/SCSP
reverse-engineering).

## Cascade / dependency map

```
                    ROM / boot     (10)
                    scheduler      (10)
                          |
              input -------+---- audio
                |                   |
                v                   v
            camera          Model 2A hw (8) - TGP execution
                          geometry / rendering (7)
                          |                |
                          v                v
                  fighter / game logic (8) - combat, physics, AI
                          |
                          v
                    complete game flow (5) - state machine
                          |
                          v
                  fully playable match (2)
                          |
                          v
                      HUD / game_disp (9) - F1, in-fight HUD
```

The "spine" of the playable match is: input → fighter →
geometry → audio → game flow → playable match.

**TGP execution** sits at the bottom of `geometry /
rendering` and `Model 2A hardware`. Without TGP
reverse-engineering, neither bar can reach 10/10.

**SCSP FM/DSP** sits at the bottom of `audio` and is
partially in `Model 2A hardware`. Same story.

These two pieces of unknown silicon (TGP microcode, SCSP
FM/DSP) are the **real ceiling** on the project reaching
10/10 on every bar.

## Phases

### Phase A — Foundations (10/10 already)

`ROM / boot`, `scheduler / runtime` are at 10/10. Done.

### Phase B — Platform (input, camera, HUD)

Small to medium slices, mostly recovery:
- B1 — host-side platform adapter (input bar to 10/10).
- B2 — in-fight camera path (camera bar to 9-10/10).
- B3 — F1 starting state + in-fight HUD (HUD bar to 10/10).

Realistic: 2-3 months of focused recovery. Tractable per
the v0771-v0773 cadence.

### Phase C — Match state machine (complete game flow)

- C1 — trace title-to-fight flow from a real snapshot.
- C2 — recover 2-3 state-machine dispatchers.
- C3 — recover transition handlers (results screen,
  rematch, return to menu).

Realistic: 2-3 months.

### Phase D — Fighter combat and AI (fighter / game logic)

The longest slice. Sub-leaves:
- D1 — physics (idle-to-crouch transition, jump, block).
- D2 — hitbox/hurtbox application.
- D3 — damage computation.
- D4 — ring-out detection.
- D5 — CPU AI decision logic.

Realistic: 4-6 months, ~5,000+ lines of recovered C.

### Phase E — Renderer and audio backend

- E1 — TGP firmware execution (reverse-engineer TGP
  microcode). **External dependency**: documented TGP
  microcode. Realistic: 6-12 months.
- E2 — Polygon rasterization. Coupled to E1.
- E3 — SCSP FM synthesis (reverse-engineer YMF262 in the
  Model 2 SCSP variant). Realistic: 3-6 months.
- E4 — SCSP DSP effects. Realistic: 2-3 months.

Phases E1-E4 require **outside** documentation work
(TGP, YMF262). Tractable but slow.

### Phase F — Integration

- F1 — wire the per-step hook for the recovered
  dispatcher chain.
- F2 — end-to-end match playthrough (title → fight →
  results) on a real ROM.
- F3 — stress / regression test pass.
- F4 — every bar measured at 10/10, single playable
  build.

Realistic: 1 month after Phases B-E.

## Total scope estimate

| Phase | Calendar time | Lines of recovered C |
|---|---|---|
| A (done) | - | - |
| B (input/camera/HUD) | 2-3 mo | ~1,500 |
| C (game flow) | 2-3 mo | ~1,000 |
| D (fighter) | 4-6 mo | ~5,000 |
| E (render/audio) | 9-18 mo | hardware-dependent |
| F (integration) | 1 mo | ~500 |
| **Total** | **18-31 months** | **~8,000+ C** |

The render/audio half (E) is the dominant cost and the
part with the most external dependency. Without TGP and
SCSP documentation, it is bounded by reverse-engineering
work that is well outside this codebase's existing scope.

## Highest-leverage moves

If only one slice can be done, the **highest-leverage**
move is the **match state machine** (Phase C) — it
unblocks fighter / game logic testing, results screen,
in-fight HUD, and is a clean ~2000-line slice.

If two slices can be done, **C (game flow) + B1
(input adapter)** together close the platform loop and
let an end-to-end match be **driven** even before the
fighter logic is fully recovered — letting every
downstream slice be tested against real button presses
instead of programmatic state injection.

## Honest scope summary

The project already has 153/153 ctests green and the
post-0x29414 player corridor fully recovered. The
remaining ~5-10% to reach 10/10 on every bar is
**disproportionately hard** because the two biggest
gaps (TGP execution and SCSP FM/DSP) are reverse
engineering of proprietary hardware, not i960 program
recovery.

For a 12-month roadmap focused on **i960-recoverable**
work only, achievable end state:
- input: 10/10
- camera: 9-10/10
- HUD / game_disp: 9-10/10
- fighter / game logic: 9/10 (combat/physics/AI mostly
  recovered, ring-out + edge cases deferred)
- complete game flow: 9/10 (state machine recovered,
  rematch flow partial)
- fully playable match: 6-7/10 (all i960-recoverable
  work done, TGP + SCSP render / audio still open)
- geometry / rendering: 7/10 (TGP microcode unknown)
- audio: 6-7/10 (SCSP FM/DSP unknown)
- Model 2A hardware: 8-9/10 (TGP execution unknown)

The project can reach **9/10 on most i960-recoverable
bars** in 12 months. The remaining 1/10 to 10/10 is gated
on the TGP and SCSP reverse-engineering work, which is
3-5 years of standalone research.

## What this plan does NOT cover

- **Reverse-engineering TGP microcode** — this is its own
  multi-year project. The MAME project has done
  partial TGP work; aligning with that would unblock
  render.
- **Reverse-engineering SCSP FM synthesis** — likewise,
  the YMF262 (and Model 2's variant) is well-known
  from other Sega arcade boards; documenting the
  Model 2 SCSP variant would unblock audio.
- **Polygon rasterization algorithm** — the renderer is
  non-trivial. Even with TGP microcode, implementing
  the rasterizer (depth, Z, texture sampling) is a
  software-engineering project of its own.

These are the project's hard external dependencies.
The i960-recoverable side of the path to 10/10 is well
within reach; the rest is a separate, larger problem.

## Status

This is a planning document. No code change. The README
bars were updated to the v0773 state in commit 93e13e92.
The next step is for the project owner to choose which
phase to prioritize, given the scope above.
