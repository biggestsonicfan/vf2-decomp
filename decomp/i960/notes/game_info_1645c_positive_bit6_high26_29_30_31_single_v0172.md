# v0172: recover positive state-8 bit-6 single high-26/29/30/31

Entry `0x0001645c` with `fighter0_state==8 && fighter1_state==8`,
`measured_matrix_distribution` and `threshold 0..2`.

The dispatcher for `bit6+bit8` plus `14/15/16` singles with an added
single high `26/29/30/31` was fail-closed. For `0x4004140` etc the
full-dispatch harness showed `-2/-4` with `EQUAL`/`LESS` and stale
`41000000 07800f0f`.

Newly admitted masks (each `combined`):

- `0x04004140` (14+26), `0x20004140` (14+29), `0x40004140` (14+30),
  `0x80004140` (14+31)
- `0x04008140` (15+26), `0x20008140` (15+29), `0x40008140` (15+30),
  `0x80008140` (15+31)
- `0x04010140` (16+26), `0x40010140` (16+30), `0x80010140` (16+31)
  (`0x20010140` (16+29) remains fail-closed: its `f1-only/bilateral`
  still `DIFF` after the same dispatcher accounting and `bit11`/`0x51105a`
  handling, and is left for next measurement).

Each admitted mask now matches the full 36-case dispatcher matrix
(3×2×2×3) via the same dispatcher deltas as the isolated high
family (`4140 +2/+4`, `8140 -3/-5`, `10140 +4/+8` with `bit11` for
`10140` family) and stale `41000000/07800f0f`.

Validation:

- `validate_game_info_full_dispatch.py --mask {04004140,20004140,
  40004140,80004140,04008140,20008140,40008140,80008140,04010140,
  40010140,80010140} --thresholds 0,1,2` each `36/36 exact` (396
  fixtures); `0x20010140` remains `12/36` and is left fail-closed
- `ctest -R vf2_native_second|fifth|sixth|seventh` still `MATCH`

Remaining: `0x20010140` and the pair/triple high extensions
(`0x24004140` etc) remain the next frontier.
