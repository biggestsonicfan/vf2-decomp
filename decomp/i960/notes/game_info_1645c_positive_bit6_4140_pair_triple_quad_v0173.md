# v0173: recover positive state-8 bit-6 4140 pair/triple/quad

Entry `0x0001645c` with `fighter0_state==8 && fighter1_state==8`,
`measured_matrix_distribution` and `threshold 0..2`.

The dispatcher for `bit6+bit8` plus `14` with an added pair/triple/quad
of high `26/29/30/31` was fail-closed. For `0x24004140` etc the
full-dispatch harness showed `-2/-4` with `EQUAL`/`LESS` and stale
`41000000/07800f0f`; the child at `0x18644` was also fail-closed via
`native_bit14_fighter_path` (which previously claimed `0x24004140`).

Newly admitted masks (each `combined`):

- pairs: `0x24004140` (26+29), `0x44004140` (26+30), `0x84004140` (26+31),
  `0x60004140` (29+30), `0xa0004140` (29+31), `0xc0004140` (30+31)
- triples: `0x64004140` (26+29+30), `0xa4004140` (26+29+31),
  `0xc4004140` (26+30+31), `0xe0004140` (29+30+31)
- quad: `0xe4004140` (26+29+30+31)

Each now matches the full 36-case dispatcher matrix via the same
dispatcher `+2/+4`, `EQUAL/LESS`, stale `41000000/07800f0f` as the
isolated high singles, and by making `native_bit14_fighter_path`
only claim isolated `0x4000` (so `0x24004140` etc use `ROM` child
at `0x18644`, which already handles the high bits via `ROM`).

Validation:

- `validate_game_info_full_dispatch.py --mask {24004140,44004140,
  84004140,60004140,a0004140,c0004140,64004140,a4004140,c4004140,
  e0004140,e4004140} --thresholds 0,1,2` each `36/36 exact` (396
  fixtures)
- `ctest -R vf2_native_second|fifth|sixth|seventh` still `MATCH`

Remaining: `8140` and `10140` families’ pair/triple/quad high
extensions remain the next frontier.
