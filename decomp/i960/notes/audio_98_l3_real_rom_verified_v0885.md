# v0885 — Original-ROM 98/98 L3 differential measured on Linux

Date: 2026-10-19. **Actual owner ROM + published native GitHub Actions
binary were executed**. This document records measured results, not a
hypothetical CI test, and keeps proprietary ROM/audio outside Git.

## Provenance

- Original owner archive: `vf2(20261018-203049).zip` (36 members).
  The four i960 program chip SHA-1s and `epr-17574.30` audio CPU SHA-1
  match `config/vf2_v22_roms.csv`. Main-data chips
  `mpr-17560.10`/`mpr-17561.11` match the manifest.
- GitHub Actions Linux artifact:
  `vf2-m68k-sound-differential-linux`, artifact **12198805910**,
  workflow run **39278161477**, source commit
  [f9274750](https://github.com/celsowm/vf2-decomp/commit/f9274750f9a38c1cb2501dd34f216f4873d06e4d).
  The published `vf2_m68k_l3_selection_tests` binary already supports
  opt-in `--l3-packet`. It **predates** the later `--matrix-l3` mode.
- The 18x11 matrix was recovered directly from the owner's original
  `mpr-17560.10` and `mpr-17561.11` chips using MAME
  `LOAD32_WORD` lane interleave; pointer table `0x0201e44c`.
  Source logic: `tools/python/audio_command_matrix.py`.

## Exact reproducible measurement

An external local-only driver enumerated 18 profiles and 11 columns,
removed zero selections and duplicate commands, then invoked the
published binary **separately for every distinct original command**:

```text
vf2_m68k_l3_selection_tests <local-unpacked-owner-roms> --l3-packet <six-hex-digits>
```

For each invocation, the acceptance predicate was **both** exit code 0
and an exact `MATCH C/68000 L3 (isolated packet fixture) 00xxxxxx:`
marker corresponding to that command. Six subprocess workers were used.
No fail or skipped packet was suppressed.

| Measurement | Result |
| --- | ---: |
| ROM-selected nonzero cells | 107 |
| Unique original 24-bit sound commands | 98 |
| AD23 table cells | 64 |
| AD24 table cells | 43 |
| Per-command L3 C/68000 differentials executed | **98** |
| Differential passes | **98** |
| Failures or mismatched command markers | **0** |
| Original sound CPU ROM hash/manifest | Matching |

This is independent **runtime evidence of the opt-in per-packet mode**
at `f9274750`. The `--matrix-l3` mode added later to
`tests/m68k/test_l3_real_rom_selections.c` should check the same
commands, but its new all-in-one executable was **not** separately
compiled/run with the owner's ROM during this measurement.

## Original engine / PCM sanity recheck

Using the same Linux artifact `vf2_m68k_90_to_voice_tests` and original
ROM, `--probe-engine` reached the original sound engine at `0xC68`
for both AD-family representative commands. The optional key-on path
activated SCSP slot 0 and the local software SCSP model produced
nonzero PCM for each command:

| Case | Original engine | PCM rendered peak | Nonzero stereo frames (4,096) |
| --- | --- | ---: | ---: |
| AD23 representative `9B 42 78` | reached `0xC68` | 3,951 | 3,949 |
| AD24 representative `9A 01 7F` | reached `0xC68` | 3,968 | 3,949 |

Both 0.5-second model WAVs were also created locally by the opt-in
`--wav-dir` command. **Do not commit those WAVs or ROM samples.**
The deeper post-copy probe ended with stepper status 8 (unsupported)
after slot activation, so this does not establish a full original
driver continuation.

## Still unproven

A pass here proves the **isolated L3 UART receiver differential** from
fresh ROM-booted sound-board state. It does **not** prove that each table
command is reached naturally during combat, nor the real i960 UART
scheduler, continuous 68000 interrupt timing, native C A0/L2/90 engine
equivalence for 98 commands, DSP silicon fidelity, or live SFX.

The highest-value next evidence is a **frame-advancing pre-contact
snapshot and passive trace** carrying one real combat command through
the existing selector, original UART and L3 chain. Without that
snapshot, ROM command enumeration cannot certify a natural punch.
