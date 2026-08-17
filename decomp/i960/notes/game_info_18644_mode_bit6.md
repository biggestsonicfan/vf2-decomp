# fa_game_info `0x18644` mode bit 6 (`base+0x3351`)

The ROM tail at `0x18898..0x188ac` uses `bbc 6`, so nonzero values with bit 6 clear are not a separate branch. A controlled `base+0x3351 = 0x01` probe matches the zero baseline at 678 instructions for the full task.

With `base+0x3351 = 0x40` and fighter bit 29 clear, the ROM executes the extra `ld (g8),r15` / `bbs 29,r15` pair and rejoins the normal tail. The full task measures 683 instructions. Isolated `0x18644` entry/return probes measure 102 -> 105 instructions for the first invocation and 97 -> 99 for the swapped second invocation, explaining the +5 total. This recovery covers that neutral bit-6-set corridor; bit29-set and combinations with the already-special state/countdown branches remain explicit unsupported boundaries.

A controlled state-bit-8 probe closes the `r7 = 0`, `r8 = 0x00000100` corridor with mode bit 6 set. The mode-clear control matches at 684 instructions; the mode-bit-6 variant matches at 689. At the two `0x18644` invocations the ROM measures 101 -> 104 instructions for the first fighter order and 97 -> 99 after the caller swaps the fighters. The bit-8 exact-state formula therefore uses the bit-8 priority count only on the first order and rejoins two instructions earlier than the generic exact-state formula there.

The same controlled fifth-dispatch snapshot was then expanded into a mode-bit-6 + fighter1-bit-8 state matrix. Exact ROM/native matches were obtained for isolated bit 8 (689 instructions), plus bit 6 (876), bit 14 (870), bit 15 (679), bit 16 (877), bit 21 (691), bit 26 (680), bit 29 (690), bit 30 (689), and the priority pairs bit 14 + bit 15 (869) and bit 15 + bit 16 (876). The single-extra-state cases use the same two-instruction earlier rejoin; the two measured priority pairs already match the generic exact-state count.

The deliberately unproved fringes remain fail-closed: bit 8 + bit 4 follows a distinct fast path, bit 8 + bit 1 enters the `0x188cc` state tree, and bit 8 present on both fighters changes the fighter-order accounting. Those cases must be recovered independently rather than inferred from the measured matrix.
