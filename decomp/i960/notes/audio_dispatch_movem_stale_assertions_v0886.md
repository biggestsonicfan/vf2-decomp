# v0886 — Re-pin stale 0xCB6 crash assertions after legal MOVEM correction

Date: 2026-10-19. User-supplied original VF2.1 set and the published
GitHub Actions Linux binary from commit `f9274750` were executed
locally. The run deliberately kept original ROM bytes, WAVs and raw
logs out of Git.

## Measurements

- `vf2_m68k_l3_selection_tests <owner-ROM-dir>`: **pass**, including
  both full A0->L2 original-driver differential legs.
- `vf2_m68k_90_ad_followup_tests <owner-ROM-dir>`: **pass**.
- All **98 unique** original AD23/AD24 command words were run using
  `vf2_m68k_l3_selection_tests ... --l3-packet XXXXXX`;
  **98/98 pass** (see `audio_98_l3_real_rom_verified_v0885.md`).
- `vf2_m68k_cdispatch_tests <owner-ROM-dir>`: **exit 1, six
  outdated assertions in three probe legs**.

The six failures, and only these six, were:

| Fixture | Old assertion | Observed original stepper result |
| --- | --- | --- |
| `test_fac_d4_crash` | PC `0xCB6` after an unbounded continuation; steps 114 | unsupported at PC `0x265C`, 20,000 steps |
| `test_engine_probe` with stop `0x09BA` | illegal MOVEM trap/PC `0xCB6` at step 384 | unsupported at PC `0x265C`, 20,000 steps |
| `test_engine_probe` with stop `0xFFFFFF` | illegal MOVEM trap/PC `0xCB6` at step 384 | unsupported at PC `0x265C`, 20,000 steps |

Each fixture asserted both obsolete PC and obsolete instruction count
(two assertions per leg). This is a **test oracle obsolescence**,
NOT evidence the legal-MOVEM ISA correction is wrong.

## Fix in master

The previous code assumed instruction `MOVEM.L D0-D7,(A2)` at
`0xCB6` (opcode `0x48D2 0x00FF`) was illegal and triggered vector
4. **That claim is false.** The Motorola M68000 programmer's manual
explicitly allows register-to-memory (An), and the independent stepper
was corrected in v0880.

`tests/m68k/test_cdispatch.c` now:

1. Explicitly runs `test_fac_d4_crash` to the **pre-instruction**
   stop `0xCB6`, asserting VF2_OK, exact PC, and 114 instructions.
2. Runs the S4 engine probe's final positive stop to `0xCB6`
   (VF2_OK, 384 instructions), instead of waiting for a nonexistent
   vector-4 exception.
3. Preserves the `0x09BA` negative reachability probe, but caps its
   reference execution at **384 instructions** and checks the
   pre-MOVEM `0xCB6` frontier. Budget exhaustion remains an explicit
   unsupported result; no unknown path is admitted.
4. Leaves the recovered-C engine refusal at `0xCB2` intact.
   The complete copy engine has NOT been promoted to recovered C.

These are minimal **test expectation fixes**. The change is committed
to `master`, but the new `vf2_m68k_cdispatch_tests` executable was
not yet rebuilt and run with the owner's ROM when this note was written.
Do **not** mark this C suite green until that independent retest
passes. The 98/98 isolated L3 result remains proven.

## Still open

One naturally triggered combat event with a frame-advancing i960 trace
and contemporaneous UART->68000->SCSP state remains necessary to
claim live game sound, despite the extensive isolated coverage.
