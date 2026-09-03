# v0240 14140 quad low variants

The `0xE4014140` quad (`bits 26+29+30+31` over `0x14140`) was the last `10×7` exclusion for `0x14140`.

## Measurement
* `0xE4014140` base `36/36` with `0/0`, low `0xE4014142` etc `0/36` with `-2` uni / `-4` bi (same as other `0x14140` high pairs)
* Same `+4/+2` + stale + bit11 as other `0x14140` high pairs — no `0x6da`

## Recovery
Extended the `0x14140` low predicate from `10` to `11` bases (`& ~0x16 == 0xE4014140` added). All `77` masks `36/36 exact` via `validate_game_info_full_dispatch.py --mask 0xE4014142`.

## Totals
* 856→863 masks (`120` + `743`)
* 55/55 CTest pass.
