# v0171: recover positive state-8 bit-6 high-21 cross family

Entry `0x0001645c` with `fighter0_state==8 && fighter1_state==8`,
`measured_matrix_distribution` and `threshold >=0` (0..2).

The dispatcher for `bit6+bit8` plus a single high `14/15/16` or
pair/triple with an added `high-21` (`0x200000`) was fail-closed.
For `threshold 0` the full-dispatch harness showed for `0x204140`
`-2/-4` with `ac 3f001000 cr0` vs `3f001002 cr2` for `f0-only cd0`
but `ac` already `EQUAL` for `f1-only`, and `cd1` `LESS` vs `NONE`
or `EQUAL`; stale at `local_frames[depth+1]` (`0x18584`) was
`84000082` vs `41000000 07800f0f`.

Newly admitted masks (each `combined`):

- `0x00204140` (14+21)
- `0x00208140` (15+21)
- `0x00210140` (16+21)
- `0x0020c140` (14+15+21)
- `0x00214140` (14+16+21)
- `0x00218140` (15+16+21)
- `0x0021c140` (14+15+16+21)

Each now matches the full 36-case dispatcher matrix
(3×2×2×3 thresholds 0..2) via the same dispatcher accounting as
the isolated high family (`v0170`): `4140/14140 +2/+4`,
`c140/1c140 +2/+5`, `8140 -3/-5`, `10140 +4/+8`, `18140 +4/+9`,
`EQUAL/LESS` on `countdown`, stale `41000000/07800f0f` etc
(`r15=1` for `f1-only/bilateral`, `r14=8` for `f0-only`);
for masks containing `16` (`10140`-family: `210140/214140/218140/
21c140`) also `fighter+0x1a4` bit11 (`0x800`) is ORed, matching the
`0x18148`-`0x18288` prefix’s `bit16` path which otherwise leaves
`0x01` vs `0x09` at `0x510b25`.

Validation:

- `validate_game_info_full_dispatch.py --mask {204140,208140,210140,
  20c140,214140,218140,21c140} --thresholds 0,1,2` each `36/36 exact`
  (252 fixtures), plus isolated high and low families still `36/36`
- `ctest -R vf2_native_second|fifth|sixth|seventh` still `MATCH`

Remaining: `high-21` with additional `high-26/29/30/31` combos
(`0x24004140` etc) remain fail-closed and are the next documented
frontier.
