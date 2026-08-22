# `fa_game_info` positive state-8 bit-14/16 high-bit triple

## Question

Can the measured bit-14 + bit-16 composition with high bits 21, 26 and 29 be
recovered at the `0x164c4` return corridor without accepting neighboring
triples?

## Controlled measurements

The calibrated `0x164ac` boundary was reused with state 8, threshold `0`, and
the standard 12-case distribution: fighter 0 only, fighter 1 only and bilateral
flags, each with countdown `0/1` and mode bit 6 `0/1`.

The mask `0x24214140` (state bit 8, bit 6, bits 14 and 16, and high bits
21+26+29) matched the reference snapshot and all instruction/call/return
counters in all 12 cases. The native correction is admitted explicitly and
uses the measured asymmetric order corrections at return `0x164c4`.

As a negative neighboring control, `0x44214140` (21+26+30) remained 3/12
exact, so the new admission does not generalize to other high-bit triples.

## Reproduction

```sh
python3 decomp/i960/tools/validate_game_info_state4.py \
  build_wsl/vf2i960 roms/vf2 --state 8 \
  --extra-bit 14 --extra-bit 16 --extra-bit 21 \
  --extra-bit 26 --extra-bit 29 --mask 248 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```
