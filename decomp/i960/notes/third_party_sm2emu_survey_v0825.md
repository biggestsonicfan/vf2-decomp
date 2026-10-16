# Survey: dmanlfc/sm2-emu -- vendoring DECLINED, kept as cross-check (v0825)

Status: surveyed 2026-10-16 from the repo README + tree
listing. No `src/`, test, or `third_party/` vendoring --
this note plus the `third_party/README.md` entry. Unlike
model2-nx (GPL repackaging, declined outright), this one is
license-compatible; the decline is on evidentiary grounds.

## What it is (source-read)

- Independent Sega Model 2 emulator (all four boards,
  2/2A/2B/2C), Linux-first + macOS/Windows, SDL +
  Vulkan/OpenGL/software renderers. 212 commits, 21 stars.
- Own i960KB core, TGP/SHARC/MB86235 coprocessors, System 24
  tilemaps, both sound boards (68000/SCSP for CRX-family
  incl. 2A, Model 1 68000/YM3438/dual-MultiPCM board).
- Stated derivation: MAME's Model 2 driver + device cores
  as research base (NOTICE attribution), Supermodel as
  renderer inspiration. Own implementation throughout.
- License: **BSD-3-Clause** (LICENSE file) -- passes the
  `third_party/` gate, unlike model2-nx's GPL-3.0.

## Why still declined for vendoring

1. **Not an oracle.** Its own README, "Known gaps": the
   geometry engine is a high-level model ("results should
   match, timing does not"); **neither sound board has been
   diffed sample for sample** (Model 1 board agrees with
   MAME "on onset and roughly on level... an envelope, not
   a waveform"; the SCSP "has not been checked to that
   depth"). An implementation without per-instruction or
   per-sample proofs cannot adjudicate anything ours
   hasn't -- our E1 legs prove C==S per effect with step
   pins; sm2-emu's sound is "produces audio", same seat as
   MAME's imperfect flag.
2. **Not a library.** Full emulator (app + renderers +
   GUI), same shape objection as model2-nx: reference-only
   reading is the only compatible use, and for that use
   MAME upstream + this tree already cover the ground.
3. **Agreement proves nothing new; disagreement is the only
   signal.** Where it matches MAME, no information. Where
   it differs, the ROM/stepper adjudicates -- worth a look
   *if* a concrete dispute arises (SCSP envelope/DSP
   behavior, TGP edge), not a standing dependency.

## Navigation leads (hearsay-grade, NOT facts)

Hardware constants its README states that rhyme with ours
and could seed future measurements: 57.5245 Hz frame
(434600 cycles @25 MHz), 496x384 frame, MIDI 31.25 kbaud
lock-free queues, audio thread clocked by the stream,
texel intensity + tone-curve chain, order-based fill mask
(no depth buffer). None of this enters `src/` or notes as
fact without in-repo measurement.

## What would reopen this

A concrete behavior dispute (ours vs MAME) where sm2-emu
holds a third, documented position -- consult that file
as a second suggestion source, ROM/stepper still decide.
