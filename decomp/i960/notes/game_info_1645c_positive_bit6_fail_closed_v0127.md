# `fa_game_info` `0x1645c`: fail-closed positive state-8 bit-6 admission

## Result

The generic positive `native_bit6_fighter_path` admission was broader than the
committed differential evidence. It accepted any non-state-4 composition with
bit 6 set and bits 14/15/16 clear, including high-bit triples/quads and mixed
low/high compositions that the recovery notes explicitly leave unproven.

v0127 narrows that gate to the masks already measured by the committed child
matrices:

- no high bits: bit 6 with any subset of low bits 1/2/4 and optional bit 8;
- one or two of high bits 21/26/29/30/31: bit 8 is required and low bits
  1/2/4 must be clear;
- all five high bits: bit 8 is required and every subset of low bits 1/2/4
  remains admitted.

The state byte must be 8 for both fighter records, the threshold must remain
nonnegative, and any flag outside the measured bit set is rejected. High-bit
triples/quads, single/pair-high masks without bit 8, and single/pair-high masks
mixed with low bits now fail closed rather than silently borrowing the generic
bit-6 corridor.

This change does not claim new game behavior. It makes the implementation match
the existing evidence boundary recorded in
`game_info_18644_positive_bit6_high_masks_v0090.md` and
`game_info_18644_positive_bit6_high_pairs_v0091.md`.

## Validation

The repository strict CMake build and CTest suite must pass before this change is
committed. The existing ROM-backed matrix tools remain the oracle for widening
this gate in a later recovery.
