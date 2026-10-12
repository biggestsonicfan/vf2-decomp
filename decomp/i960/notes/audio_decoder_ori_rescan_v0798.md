# v0798 - decoder ORI/EORI + frontier rescan (slice 1)

## Handler

`src/m68k/decoder.c` had no ORI/EORI. The v0797 "movep gap" at 0x49e
is `ori.b #$10,d7` (opcode `0x0007` + imm). Added ORI/EORI
`#imm,ea` for B/W/L with modes 0/2/3/4/5/6 + CCR/SR specials;
mode 6 uses the v0797 DA-correct brief-ext print. Other modes mark
unsupported explicitly (better than ANDI's silent fallthrough,
which is left untouched as out of scope).

Vectors (`tests/m68k/test_decoder.c`, `ori_eori` array): the ROM
`0x49e` pair, CCR, EORI.Dn, indexed, predecrement. Gate: old code +
new vectors FAILs (observed pre-fix `.word` prints); fixed code
green.

## Rescan dispositions (fixed decoder, linear sweeps)

- Boot chain + inits (0x100-0x330): only data words (0x6f4/0x714
  ISR-table tail). No recovered claim touched.
- Primer (0x858-0x8d0), e0 tail (0x1356), C0/0x1de0, ISR, L3,
  0x28e: clean. v0794a/v0794b/v0795/v0797 reads stand.
- L3 commit sub now reads `ori.b #$10,d7` at 0x4b8: cosmetic.
- NEW visibility, drain voice-setup block 0xc30-0xcb6 (+0xd98
  twin): `ori.l #$10300000,d6` + `move.l d6,$4000(a5)` (SCSP+0x4000
  window), ram[0x600a]/ram[0x2000+d4+1]/ram[0x601a]/ram[0x601c]
  voice fill. Post-refusal (no C refuse point reaches it; C0 path
  ends at its 0x1de0 loop, e0 at its 0x13c8 rts). Entry mapping +
  SCSP+0x4000-window semantics go to slices 3/4.
- Unresolved mix around 0xc46-0xc68 (code/data interleave, branch
  targets only): `$00a0` now reads as an ORI.B imm byte at an
  aligned `ori.b #$a0,-(a2)` (0xc46); `$463c`/`$2c0a`/table words
  pending entry-mapped reading in slice 4. No recovered claim
  depends on this region (all C refuse points precede it).

## Standing rule (from the plan)

68k recoveries are static-match, not differential-proven: no 68k
execution oracle exists. Byte-level ea verification per site +
planted gates (which prove only that tests can fail) remain the
acceptance bar. The §5 honesty sentence lands in slice 5.
