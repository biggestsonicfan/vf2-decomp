# v0799f - e0 tail is d4-indexed, not d0-scattered (correction)

## What was wrong

The v0799b e0 fix read 0x1396 (`3b 80 40 10`) as
`move.w d0,$10(a5,d0.w)` (d0-scattered far write with VDP1-style
tolerance). Re-reading with the proven-fresh decoder plus a second
hand-decode of the ext word (0x4010>>12 = 4, not 0 — the third
instance of the same shift slip): it is
`move.w d0,$10(a5,d4.w)`, d4 = voice[0] slot word. The value is
d0 (shaped); the stride is d4. The fresh tool prints `$10(a5,d4.w)`
and byte-verified 0x13c4 agrees (`move.w d2,$1a(a2,d4.w)`).

## Fix

- SCSP[d4+0x10] = shaped (always in-file for live slot words;
  odd slot word fails closed as address-error shape; hostile
  beyond-file fails closed via write_u16 propagation). The
  VDP1-tolerance branch is deleted with the scatter theory.
- ram[0x201a+d4] unchanged (was already right).
- Fixtures now pin d4 = 0x0020 (nonzero slot word, which the e0
  match logic never consults): SCSP[0x0030] = shaped,
  ram[0x203a/b] = stored. A d4 of 0 would coincide with the old
  absolute reading, so the nonzero word is the load-bearing part.
- 2 plants fail exactly: absolute SCSP+0x10 (both SCSP asserts),
  absolute ram[0x601a] (all four ram asserts). The SCSP plant is
  the regression guard against both the absolute fiction and the
  scatter misread.

## Notes amended by reference (files untouched)

- v0799b's "SCSP[d0+0x10] far write / VDP1-style" paragraphs are
  superseded by the above; its ram[0x201a+d4], KEYONB/SA sweep,
  and +0x4000-retraction conclusions stand (0xc8e re-verified as
  `$00(a5,d4.w)` on the fresh binary).
