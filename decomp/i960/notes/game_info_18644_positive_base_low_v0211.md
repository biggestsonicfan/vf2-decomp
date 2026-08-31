# v0211 base low-bit completion (0x4140 and 0x14140)

Completes the low-bit cube for the no-high and single-high-16 families
on the bit-6+14 base.

## Families
* `0x00004140` (base, no high) — 1→8 masks (low 1,2,4)
* `0x00014140` (high 16) — 1→8 masks

Each family expanded from 1–2 masks to 4 blocks covering 8 low variants:
`0x...40,42,44,46,50,52,54,56` (low bits 1,2,4). Each `+4 bilateral/+2 unilateral`
same as v0205.

## Measurement
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the 16 masks,
  each 36/36 exact (3 distributions ×2 countdown ×2 mode6 ×3 thresholds).
* Before, only `0x00004140` base and `0x00014140` were admitted; 14 low variants were `0/36 DIFF -4`.

Together with v0205-v0210, the entire `0x4140` high family over
26,29,30,31 plus the no-high base and high-16 are now low-complete
(16+120=136 masks).

55/55 CTest pass.
