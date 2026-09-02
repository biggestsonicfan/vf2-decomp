# v0226 high pairs for 0x8140

Admits 6 pair masks for base 0x8140 over high 26,29,30,31.

## Measurement
* Each `0x24008140`, `0x44008140`, `0x84008140`, `0x60008140`, `0xA0008140`, `0xC0008140` `36/36 exact` with `0` excess (no subtraction) and `hybrid_set_stale_low`
* Low variants `|0x02` like `0x24008142` remain `0/36` with `+5` diff, stayed fail-closed (low cube for pairs not admitted)

## Recovery
6 exact `==` predicates, total +6 masks (208→214).

55/55 CTest pass.
