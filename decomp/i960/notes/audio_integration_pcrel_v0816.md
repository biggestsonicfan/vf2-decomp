# v0816 - kazuyette merge: pcrel +2 base flips the audio map

## What was pulled (origin/master 6a57654d..8494a866, 11 commits)

- **c855eea6 (THE load-bearing one)**: 68000 PC-relative base is the
  extension-word address (opcode+2), not opcode+4 — for Bcc.w,
  BRA/BSR.w, DBcc, JSR/JMP (d16,PC)/(d8,PC,Xn). ROM-measured
  (176/176 Bcc.w land on starts vs 31) + MAME-confirmed (hot loop
  0x1f88). Decoder + stepper fixed, ROM-independent gate
  (`test_pcrel_base`), reaudit note. Our local tree had the +4 bug
  in every slice v0808-v0815.
- **8c18a1f7/d6211d8d**: voice-maintenance ISR re-audit under +2
  (32 voices serviced, release arm live) + MAME d2-residue
  measurement (several hundred distinct entry values — a faithful
  C must take d2 as input).
- **d8cb02cd**: sample directory located (650x20B @0x5b28 ->
  RAM 0x5000; evidence note).
- **01949d9d**: two sanitizer fixes (test fixture over-read,
  hybrid.c shift-UB mask). Merged cleanly.
- **2c72dcfa**: gcc13/clang18 -Werror fixes. One hunk broke MSVC
  (`__attribute__((unused))`); guarded with `#if defined(__GNUC__)`
  (follow-up commit here). Everything else merged cleanly.
- **a3ace03b/b3073ac7/8494a866**: playable-build plan, CI gate +
  rom_gate scripts, SDL3 host shell (VF2_BUILD_HOST off by
  default — default build unaffected).

Merge commit 3a65c463. Only conflict: CMakeLists (our midi/
dispatch registrations + their WILL_FAIL block — both kept).

## Damage to local slices (quantified, not hand-waved)

Every word-displacement target shifts by exactly -2; short
branches are unaffected. Re-measured against the fixed stepper:

- v0813 L3 legs: leg 1 path moves (beq.w $5ec, no push); leg 2
  exit moves (bra.w $5e2, push/restore sequence changed). Both
  FAIL as pinned. The Dn->-(An) decoder fix and u64-wrap fix are
  unaffected (non-PC-relative).
- v0814 dispatch legs: 6 of 7 FAIL as pinned (leg 2 hits at the
  right pc with the wrong count: 5 not 4 — the epilogue now
  routes through HEAD). Only the short-branch-only content
  survives unscathed.
- v0808-v0812 crash sites: the SITES were reached through word
  branches, so every site pc is suspect; kazuyette's reaudit
  already shows 0x7bc was a mid-instruction landing (true drain
  entry 0x7a6 runs 3000 steps clean).

## Verdict flips (byte-verified under +2)

- v0814 "flag coupling" (epilogue-Z -> drain-bne -> overlap):
  RETRACTED, was a +4 artefact. Epilogue jumps to HEAD 0x2242
  (fresh flags); walk jumps to drain-HEAD 0x7a6 (fresh (a4)).
  The circuit is flag-clean.
- v0814 "0x7b2 overlap arm" (ori.l/ori.b -> 0x7bc fault):
  RETRACTED, was a +4 mid-movem landing. True landing is the
  legal push path 0x7b0.
- v0811 "drain-HEAD dead": FLIPPED to live (walk's jmp targets
  0x7a6). v0811 "MID live": flipped to dead-or-orphaned (0x7a8
  reachable only via 0x225c — recompute pending).
- v0813 "$0408 write dead" (0x265c): FLIPPED to live — drain
  exits (0x7ac etc.) target 0x265C under +2, flowing THROUGH the
  write. The MIDI-echo hypothesis gains a live writer.
- v0814 "beq $2670 never taken, init-lea dead": UPHELD on flags
  grounds (ori.b #$8 forces Z=0 regardless of base), new target
  0x266e.
- v0814 "prescaler forwards to walk": RETRACTED — 0x21be/0x21c6
  target 0x2240 `rts` under +2. The L2->walk link evaporates;
  the TRUE prescaler is a countdown gate ending in return.
- v0814 "boot dispatches the walk": UPHELD, sharpened — 0x21e
  targets HEAD 0x2242 (not MID). Boot is now the ONLY known
  walk driver. Who drives the walk in production is fully open
  (was S4, now the central question).
- Loop-B "dead" (2ea0e618 verdict): already retracted by
  kazuyette (alive: 32-iteration scan). Our slices never
  depended on it.

## Version-number collision (do not "fix", just record)

Both lines numbered independently: remote v0805 = pcrel fix,
local v0805 = silicon audit; remote v0806 = sample directory,
local v0806 = oracle save/resume. Filenames differ, git is
happy, humans must read filenames not versions. Next local
notes continue v0817+; no renumbering of history.

## Re-measurement worklist (supsersedes v0815 S1-S10 ordering)

- R1. Re-pin L3 MIDI legs under +2 (reset path w/o push;
  file exit via 0x5e2; re-derive the shuffle).
- R2. Re-derive the walk/drain/epilogue map under +2 (HEAD
  entries, 0x7a6 drain, 0x265c write live) and re-pin all 7
  dispatch legs; the active arm now starts 0x224c (move.w).
- R3. Re-visit v0808-v0810 sites under +2 (entries shift;
  some "crashes" may become legal code, some stay).
- R4. Production walk driver hunt (was S4, now central):
  with L2->walk gone, find what calls the walk in the live
  system — main-loop scan (kazuyette left "0x2242 entries"
  explicitly not re-audited).
- R5. C voice-scan re-recovery (their WILL_FAIL; our S10
  depends on it). Coordinate, don't duplicate: voice ISR is
  their slice, walk/drain/L3 is ours.
- R6. v0808-v0815 notes get "+4-era" headers pointing here;
  docs/UNCOVERED_BRANCHES triptych gets a correction pass.

## Lessons

- A wrong-but-self-consistent instrument passes every internal
  check: our stop-pc HITs PROVED the +4 targets because the
  stepper shared the bug. Only an outside oracle (MAME run,
  landing census 176/176) can break that circle. Trust
  instrument-internal agreement exactly as far as the
  instrument's own validation — no further.
- kazuyette's landing census is the model for cheap decisive
  tests: count landing-on-start over the ROM before believing
  any base/offset convention. Adopt for future conventions.
