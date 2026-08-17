# fa_game_info `0x18644` mode bit 6 (`base+0x3351`)

The ROM tail at `0x18898..0x188ac` uses `bbc 6`, so nonzero values with bit 6 clear are not a separate branch. A controlled `base+0x3351 = 0x01` probe matches the zero baseline at 678 instructions for the full task.

With `base+0x3351 = 0x40` and fighter bit 29 clear, the ROM executes the extra `ld (g8),r15` / `bbs 29,r15` pair and rejoins the normal tail. The full task measures 683 instructions. Isolated `0x18644` entry/return probes measure 102 -> 105 instructions for the first invocation and 97 -> 99 for the swapped second invocation, explaining the +5 total. This recovery covers that neutral bit-6-set corridor; bit29-set and combinations with the already-special state/countdown branches remain explicit unsupported boundaries.

A controlled state-bit-8 probe now also closes the isolated `r7 = 0``,
`r8 = 0x00000100` corridor with mode bit 6 set. The mode-clear control
matches at 684 instructions; the mode-bit-6 variant matches at 689. At the
two `0x18644` invocations the ROM measures 101 -> 104 instructions for the
first fighter order and 97 -> 99 after the caller swaps the fighters. The
bit-8 exact-state formula therefore uses the bit-8 priority count only on
the first order and rejoins two instructions earlier than the generic
exact-state formula there. Mixed bit-8 state combinations (including bit 1,
bit 14 and bit 8 on both fighters) remain fail-closed pending pairwise
recovery.
