# Survey: toniisound/model2-nx -- vendoring DECLINED (v0825)

Status: surveyed 2026-10-16 from the repo itself (README +
file tree + license tags), not vendored. No `src/`,
test, or `third_party/` change beyond this note and the
one-paragraph `third_party/README.md` entry.

## What it is (source-verified)

- Sega Model 2 arcade emulator for Nintendo Switch
  (homebrew `.nro`), by ToniiSound.
- Contents: a MAME 0.289 subset (`mame/`, imported by
  `scripts/import_mame.py`, Switch changes marked) + Switch
  platform layer (`switch/`, `osd/nx/`, from SuperModel NX)
  + bundled configs. ~1,100 MAME files compiled.
- Sound path: MAME's 68k CPU + MAME's SCSP core driving
  MAME's Model 2 driver. No original sound research, no new
  dumps, no second implementation -- "Powered by MAME" is
  the repo's own description; credits name the MAME team
  for driver and CPU cores.

## License (source-verified)

- Repo-level: **GPL-3.0-or-later** (LICENSE file, README
  "License" section).
- The `mame/` files keep their own licenses (mostly
  BSD-3-Clause, per the README) -- i.e. the MAME *files*
  are BSD-pickable exactly as `third_party/` already does,
  but the *project* (and any of its own code) is GPL-3.0.

## Why declined (three independent blocks, any one suffices)

1. **License.** `third_party/README.md` admits BSD-3-Clause
   MAME files as reference-only, never linked. GPL-3.0
   fails that gate; vendoring would poison the
   portable-C17 clean-room goal.
2. **No new evidence.** Its sound is MAME's SCSP core. Our
   hierarchy is ROM -> stepper oracle -> differential
   proof; a MAME repackaging can only sit in the "suggests"
   seat, which MAME upstream already occupies (same
   BSD-pickable files, no Switch scaffolding).
3. **Disqualified as ground truth.** MAME flags imperfect
   sound on all SCSP games (upstream commit `5aa8d67`) with
   open Model 2 SCSP bugs (DOA corruption, sega rally).
   An imperfect model must never be oracle or debug truth
   -- the exact trap AGENTS.md's framework rule bars.

## What would reopen this

Original SCSP/DSP/UART research published there (new
measurements, not a new MAME repackaging) -- re-survey
then, BSD-pickable files only, same reference-only rules.
