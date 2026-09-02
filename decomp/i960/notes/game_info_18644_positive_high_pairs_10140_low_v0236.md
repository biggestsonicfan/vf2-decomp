# v0236 high pairs low variants for 0x10140

Extends the `0` excess high-pair admissions (v0228) to their full low cubes.

## Measurement
* Base `0x24010140` etc are `36/36` with `0/0`, low variants `|low` where `low∈{0x02,0x04,0x06,0x10,0x12,0x14,0x16}` are `0/36`
* Clustering by bit29:
  - with bit29 (7 bases: `0x24010140`, `0x60010140`, `0xA0010140`, `0x64010140`, `0xA4010140`, `0xE0010140`, `0xE4010140`) → `-3` unilateral / `-6` bilateral and `fighter+0x6da=0x1e` vs `0x00`
  - without bit29 (4 bases: `0x44010140`, `0x84010140`, `0xC0010140`, `0xC4010140`) → `-4`/`-8`, no `0x6da` write
* Pattern matches isolated high-29 vs high-30/26 singles (v0235 vs v0224/v0225)

## Recovery
Two compact predicates over `& ~0x16` plus `& 0x16 !=0`:

* 7×7=49 masks with `+6/+3` + `0x1e` for bit29
* 4×7=28 masks with `+8/+4`

plus stale `hybrid_set_stale_low` and `fighter+0x1a4` bit11. All `77` masks `36/36 exact`.

Base `0x24010140` etc remain `0` excess; low variant predicate requires `low!=0` so no overlap.

## Totals
* 527→604 masks (`120` high family + `484`)
* 55/55 CTest pass.
