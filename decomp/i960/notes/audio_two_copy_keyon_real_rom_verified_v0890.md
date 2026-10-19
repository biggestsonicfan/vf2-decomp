# v0890 — Measured 4/4 real-ROM differential: both 68000 sample-upload branches

Date: 2026-10-19. **Four real executable differentials were run**,
not inferred from CI status. No ROM, samples or WAV were committed.

## Execution provenance

- Owner-provided original archive: `vf2(20261018-203049).zip`,
  unpacked locally; proprietary files remain outside this repository.
- Actual GitHub Actions GCC artifact
  `vf2-m68k-sound-differential-linux`, artifact **12214924375**,
  workflow run [CI #2125](https://github.com/celsowm/vf2-decomp/actions/runs/39310441583),
  commit [cdd59c6d9b](https://github.com/celsowm/vf2-decomp/commit/cdd59c6d9b080b721c1f13319572dc2ad95fd1d8).
- Native binary `vf2_m68k_engine_copy_page_tests` executed with
  directory of all owner ROM chips as sole positional argument.
- Exit code **0**.

Exact runtime acceptance output:

```text
MATCH 68000/C 00cb2..00d90 select=0242 source=00e631b2 bytes=512 instructions=55 flag=80
MATCH 68000/C 00cb2..00d90 select=0261 source=00f04a72 bytes=512 instructions=55 flag=80
MATCH 68000/C 00df8..00ed6 select=0242 source=00e631b2 bytes=512 instructions=55 flag=81
MATCH 68000/C 00df8..00ed6 select=0261 source=00f04a72 bytes=512 instructions=55 flag=81
MOVEM+KEYON: two original 55-instruction copy/tail paths PASS
```

Each of four differential cases independently runs exactly
**48 original MOVEM+ADDA instructions** for 512 bytes, and then
**7 original Motorola 68000 tail instructions** (220 original CPU
instructions over the four separate executions, not one game session).
The test verifies full 512-KiB sound RAM, modeled SCSP state,
A0/A2 and final loaded D0..D7, along with the correct branch-specific
`0x80` or `0x81` stream status flag. ROM-free failure-safety
tests also ran successfully in CI's GCC build.

The original `epr-17574.30` 0xCB2 and 0xDF8 16x opcode
triplets and both 30-byte tails were byte-compared independently
against the original owner's ROM in the local runtime before
replaying the executable.

## Interpretation and next frontier

This establishes native-C parity with the original 68000 for **both
512-byte copy+KEYON branches** in deliberately isolated fixtures.
It does **not** establish that the channel reaches either branch
in natural gameplay, nor time-accurate IRQ/UART, final audio DAC,
SCSP DSP fidelity, or correct post-RTS dispatch continuation.

The next high-value task is to recover/verify the entry-state
contract of the `0xD92` alternate setup arm and bridge into
full original dispatcher lifecycle while maintaining fail-closed
guards where continuous session state is not proven.
