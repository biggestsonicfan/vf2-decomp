# v0237 high pairs low variants for 0x18140

Extends `0` excess high-pair admissions (v0230) to full low cubes.

## Measurement
* Base `0x24018140` etc `36/36` with `0/0`, low variants `|0x02` etc `0/36`
* Clustering by bit29: with bit29 → `-3` uni / `-7` bi + `fighter+0x6da=0x1e` + `fighter+0x1a5=0x08` (bit11), without → `-4`/`-9` + bit11 only
* Unilateral already `+3/+4` gives `24/36`, bilateral needs `+1` extra → `+7/+9` gives `36/36`

## Recovery
Two predicates over `& ~0x16` plus `low!=0`:

* 7 bases with bit29: `0x24018140`, `0x60018140`, `0xA0018140`, `0x64018140`, `0xA4018140`, `0xE0018140`, `0xE4018140` → `+7/+3` + `0x1e`
* 4 bases without: `0x44018140`, `0x84018140`, `0xC0018140`, `0xC4018140` → `+9/+4`

plus stale and bit11. All `77` masks `36/36 exact`.

## Totals
* 604→681 masks (`120` + `561`)
* 55/55 CTest pass.
