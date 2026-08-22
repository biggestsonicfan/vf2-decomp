# `fa_game_info` `0x18644`: positive state-8 bit-6/bit-14 high-bit extensions

The measured positive-threshold fighter fields combining state bit 8, fighter
bits 6 and 14, and one of high bits 26, 29, 30 or 31 were recovered at the
`0x18644` child:

| High bit | Mask |
| --- | ---: |
| 26 | `0x4004140` |
| 29 | `0x20004140` |
| 30 | `0x40004140` |
| 31 | `0x80004140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per mask, 48 cases total. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits only one of these four high bits with bits 6 and 14.
The measured first-order, second-order and bilateral joins use the same
high-bit accounting rule. Multiple high bits in this cross-family composition
remain explicit unsupported boundaries.

Reproduction for each high bit:

```bash
for bit in 26 29 30 31; do
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms \
    --state 8 --include-bit8 --extra-bit 6 --extra-bit 14 \
    --extra-bit "$bit" --mask 120 --threshold 0 \
    --base out/state8-positive.boundary.vf2snap
done
```

Expected result: `summary: 12/12 exact` for every iteration.
