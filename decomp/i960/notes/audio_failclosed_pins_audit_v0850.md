# Audio fail-closed pins audit (v0850)

Status: **13 planted fail-open mutations** across the sound model.
8 were already pinned (the refusal's removal fails a specific leg).
**5 boundaries carried no pin at all** — their removal was undetected.
Four of the five now have C-level pins in `test_sound_board.c`
(+142 lines); each new pin was proven able to fail by re-planting
its mutation. The fifth (session pump unattached guard) has no
reachable test state through the public API and is documented as
unpinnable. **No `src/` semantics changed in this commit** — the
audit is test-only, so the frozen behavior is provably the audited
behavior.

## Method

For each fail-closed boundary: apply a minimal *fail-open*
mutation to the C (the refusal silently succeeds instead), rebuild,
run the sound detector set, record which leg fails, revert. The
detector set is the 19 ctest entries whose binaries can reach the
boundary — **every test file that calls the sound board / session
pump / `vf2_game_update` APIs** (`test_sound_board.c`,
`test_cdispatch.c`, `test_triangle.c`, `test_session_audio.c`,
`test_game.c`; plus the scsp/m68k/drainer/session entries), so an
"undetected" result cannot be an artifact of running too few
tests. Set runs in ~5 s; each build ~25 s.

Runner (scratch, not committed): `out/pins_audit/run.ps1 <tag>`
→ `out/pins_audit/<tag>.ctest.log`.

## Results

| # | Boundary | Planted mutation | Detected by (first assert) |
|---|---|---|---|
| M1 | a0 join refusal `0x1e98` (`sound_board.c:2550`) | UNSUPPORTED→OK | `test_sound_board.c:698/703/704`; `test_cdispatch.c:2608/2611/2613/3932`; both pump ctests |
| M5 | keyon arm door `0xcb2` (`:1953`) | UNSUPPORTED→OK | cdispatch 1217–4117 (23 asserts, 8 legs) |
| M6 | d92 arm door `0xdf8` (`:1889`) | UNSUPPORTED→OK | cdispatch 1686/1688/1689 |
| M7 | allocator found-voice `0xfac` (`:2267`) | UNSUPPORTED→OK | sound_board 402–548 (10 asserts); cdispatch 12+; both pump ctests |
| M8 | 90-class `second == 0` trap (`:2390`) | remove | sound_board 366/371; cdispatch 696 |
| M9 | `file_uart_byte` 0xff reboot door (`:2044`) | remove (falls to the >=0xf0 ignore) | cdispatch 3306 |
| M10 | stager `0x6ffff` overrun halt (`:294`) | remove | sound_board 1534 |
| M13 | `vf2_game_update` oracle preflight (`src/game/game.c:540`) | remove | `vf2_game` (`test_game.c:80`) |
| M2 | odd address in `read_table_word` (`:1109`) | remove guard | **∅ unpinned → PIN ADDED** |
| M3 | odd slot in `setup_author_at` (`:1625`) | mask even | **∅ unpinned → PIN ADDED** |
| M4 | odd slot in the e0 voice tail (`:2613`) | remove guard | **∅ unpinned → PIN ADDED** |
| M11 | stager bus refusals (`:229` + `:242`) | remove both | **∅ unpinned → TWO PINS ADDED** |
| M12 | pump unattached guard (`src/app/session.c:386`) | UNSUPPORTED→OK | **∅ unpinned, not pinnable (see below)** |

Line numbers are the first assertion(s) that fired; more follow.
The M5/M7 "measured-part-then-refuse" doors hold the packet, so
their pins are the triple (status UNSUPPORTED, count unchanged,
cursor unchanged) — a state-only check would pass a bad fix, the
counter check is load-bearing.

## The four new pins (`tests/hardware/test_sound_board.c`)

All four are C-level public-API legs; the m68k stepper has **no
address-error / unmapped-read model** for these shapes
(`test_cdispatch.c:2394` documents it), so C-vs-stepper
differentials *cannot* pin them. That is exactly why they were
unpinned: every one of the five holes is an address-error-shape or
guard-only refusal, the class the differential corpus cannot
reach by construction.

- `:607` **setup-author odd slot** — voice0 slot word 0x0001, flags
  pre-cleared; `vf2_sound_board_setup_author(board,0,0)` must return
  UNSUPPORTED and leave the flags byte clear (refusal precedes all
  authoring). Re-plant (M3) fails at 620/621.
- `:731` **a0 odd row word** — dir[1] = 0x00607401 (odd); the head's
  `move.w` shape must refuse at the 0x1e74 handler with the packet
  held. This pins the shared `read_table_word` guard for all 10 call
  sites. Re-plant (M2) fails at 751/753/754.
- `:999` **e0 voice-tail odd slot** — same small-shaped e0 fixture as
  the OK leg with slot word 0x0021; must refuse (handler 0x12b6,
  voices_matched 0) before the SCSP write — slot+0x10 (0x0030)
  stays clear. Re-plant (M4) fails at 1039.
- `:1121` **stager bus refusals (two fixtures)** — row 0x005FFFFE
  makes the record-total read fail while its +2 selector read would
  land on the ROM base, pinning guard 1 exactly; row 0x0007FFFE
  (RAM top) passes the record-total read and pins guard 2 exactly.
  Both fixtures poison the row *after* `initialize()` because the
  boot uploader runs the same stager. Re-plant (M11) fails at
  1149 and 1168/1169/1170.

Mutation re-plants were verified against the new pins and reverted;
the committed tree is byte-identical to the audited (guard-present)
tree apart from the test file.

## The one unpinnable boundary (M12)

`vf2_session_pump_audio` refuses `UNSUPPORTED` when
`!s->audio_ready || s->game.sound == NULL` (`session.c:386`).
No public constructor can produce a session with a failed audio
attach: `vf2_session_open` builds every region from a supported ROM
directory and attaches it (`session.c:108`); a bad directory fails
the open outright, and `struct vf2_session` is opaque. With the
real supported ROM set there is no reachable test state for this
guard, so its removal would be undetected. Its render-side twin
(`vf2_session_render_audio`, `:347`) deliberately zero-fills and
returns OK when not ready — that is documented behavior, not a
refusal, and is exercised by the plumbing leg.

## What this audit does not claim

- Detection is within the 19-entry detector set (which covers every
  caller of the audited APIs); other ctest entries do not link these
  paths.
- M1..M13 are single-boundary mutations, not an exhaustive fuzz of
  the model; per-op `OUT_OF_BOUNDS` bounds checks are covered
  separately by the C-vs-stepper differentials (status equivalence),
  not audited here.
- No audible/DSP-execution claim is touched; P5 stands unchanged.

## Reproduction

```
powershell -File out/pins_audit/run.ps1 baseline      # 19/19
# per mutation: apply the edit named above, then
cmake --build build --config Debug --parallel
powershell -File out/pins_audit/run.ps1 M<N>
git checkout -- src/hardware/sound_board.c            # revert
```

Full suite on the frozen tree: see commit message (observed on the
committed tree, `scripts/ctest-fast.ps1`).
