# Scaffolding register

Everything in this file is temporary. Each entry names the phase of
`docs/NATIVE_WINDOWS_PLAN.md` that removes it. An entry may only be deleted by
the commit that removes the code it describes.

| Item | Where | Why it exists | Removed by |
|------|-------|---------------|------------|
| Snapshot-only start | `vf2_session_open`, `vf2_host --snapshot` | The runtime only advances from a proven i960 snapshot; cold boot is not available | Phase 2 |
| Interpreter fallback in the hybrid runtime | `src/recovered/hybrid.c` | Unrecovered regions still run through the i960 interpreter | Phases 3-5 |
| Software rasteriser as the only video producer | `vf2_platform_*`, `vf2_session_pixels` | Reference output while the GPU renderer is built | Phase 7 (kept as the test oracle) |
| Audio ring fed with the sound-board render | `vf2_host_audio_ring`, `host_push_audio` | Seam for the native mixer; the 68000 driver is not yet recoded | Phase 8 |
| Fixed 496x384 output size | `VF2_SESSION_WIDTH/HEIGHT` | Matches the current framebuffer | Phase 7 |
| Default key bindings | `vf2_host_bindings_defaults` | Replaced by the user's ini via `--bindings` when present | never (stays as defaults) |

## Host seam

`include/vf2/session.h` is the only entry the host uses to run the game:
`vf2_session_open` / `step` / `pixels` / `render_audio` / `close`.
`include/vf2/host.h` holds the SDL-free pieces (bindings, held-input tracker,
audio ring, aspect fitting) and is unit-tested without SDL. `src/host/host_sdl.c`
is the only file that includes SDL headers.

## Binding file

`--bindings file.ini`, one `source = BUTTON` per line, `#` or `;` comments.
Sources are SDL scancode names (`Z`, `Left`, `Return`) or `pad:` plus an SDL
gamepad button string (`pad:a`, `pad:dpup`). Buttons are the
`VF2_PLATFORM_BUTTON_*` names without the prefix (`PUNCH`, `P2_KICK`, ...).
A bad line rejects the whole file.
