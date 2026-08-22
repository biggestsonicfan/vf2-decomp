# `fa_game_info` `0x18644`: positive state-8 bit-2/bit-6 composition

The measured positive-threshold fighter fields `0x144` (state bit 8 plus
fighter flag bits 2 and 6) were recovered at the `0x18644` child. The
second-order mode-bit-6 guard now admits the isolated `0/0x144` and
`0x144/0` cases, and the bilateral `0x144/0x144` admission is exact as well.

The focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases total. Every case matched the ROM at
the scheduler boundary, including CPU state, condition state, mutable memory
and instruction/call/return counters.

Per-call counter deltas were measured before implementation. The no-countdown
isolated paths require distinct first-order corrections for the two fighter
orders and distinct second-order corrections after the dispatcher swaps the
arguments. The bilateral path has separate first-order and second-order
rejoin corrections for mode bit 6 and countdown. These corrections are
restricted to the exact `0x144` compositions. The neighboring `0x150`
composition is documented separately in
`game_info_18644_positive_bit6_bit8_v0073.md`; the next `0x146` composition is
documented in `game_info_18644_positive_bit6_bit8_v0074.md`.

Reproduction:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms \
  --state 8 --include-bit8 --extra-bit 6 --mask 26 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

Expected result: `summary: 12/12 exact`.
