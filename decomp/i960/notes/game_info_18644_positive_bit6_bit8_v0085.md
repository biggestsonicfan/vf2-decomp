# `fa_game_info` `0x18644`: positive state-8 bit-6/bit-16/bit-21 composition

The measured positive-threshold fighter fields `0x210140` (state bit 8 plus
fighter flag bits 6, 16 and 21) were recovered at the `0x18644` child. The
second-order guard and bilateral admission now cover the isolated and
bilateral compositions; the bilateral return-body accounting includes the
measured mode-bit-6 and countdown join.

The focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases total. Every case matched the ROM at
the scheduler boundary, including CPU state, condition state, mutable memory
and instruction/call/return counters.

Reproduction:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms \
  --state 8 --include-bit8 --extra-bit 6 --extra-bit 16 --extra-bit 21 \
  --mask 120 --threshold 0 --base out/state8-positive.boundary.vf2snap
```

Expected result: `summary: 12/12 exact`.
