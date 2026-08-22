# `fa_game_info` `0x18644`: positive state-8 bit-1/bit-2/bit-6 composition

The measured positive-threshold fighter fields `0x146` (state bit 8 plus
fighter flag bits 1, 2 and 6) were recovered at the `0x18644` child. The
existing child corridor was instruction-short in every no-countdown case;
per-call ROM comparison identified the physical-order and mode-bit-6 rejoin
deltas and those corrections are now restricted to the exact `0x146` pair.

The focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases total. Every case matched the ROM at
the scheduler boundary, including CPU state, condition state, mutable memory
and instruction/call/return counters.

The neighboring measured `0x152` composition is also exact across its 12-case
matrix. The `0x154` and `0x156` low-bit compositions are now exact as well;
the next frontier must be selected from a measured high-bit combination.

Reproduction:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms \
  --state 8 --include-bit8 --extra-bit 6 --mask 27 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

Expected result: `summary: 12/12 exact`.
