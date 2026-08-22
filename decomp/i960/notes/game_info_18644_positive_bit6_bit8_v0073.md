# `fa_game_info` `0x18644`: positive state-8 bit-4/bit-6 composition

The measured positive-threshold fighter fields `0x150` (state bit 8 plus
fighter flag bits 4 and 6) were recovered at the `0x18644` child. The
second-order mode-bit-6 guard now admits the isolated `0/0x150` and
`0x150/0` cases, and the bilateral `0x150/0x150` admission is exact as well.

The focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases total. Every case matched the ROM at
the scheduler boundary, including CPU state, condition state, mutable memory
and instruction/call/return counters.

The no-countdown isolated and bilateral paths required distinct measured
rejoin corrections at the two child return sites. Those corrections are
restricted to the exact no-high `0x150` composition. The adjacent `0x146`
composition is documented in
`game_info_18644_positive_bit6_bit8_v0074.md`; the next incomplete composition
is `0x154`.

Reproduction:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms \
  --state 8 --include-bit8 --extra-bit 6 --mask 28 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

Expected result: `summary: 12/12 exact`.
