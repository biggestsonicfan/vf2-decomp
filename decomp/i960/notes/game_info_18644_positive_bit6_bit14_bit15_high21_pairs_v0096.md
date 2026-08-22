# `fa_game_info` `0x18644`: positive state-8 bit-21 high-bit pairs

The four measured combinations of high bit 21 with one of high bits 26, 29,
30 or 31 were measured with state bit 8 and fighter bits 6, 14 and 15:

| High-bit pair | Mask |
| --- | ---: |
| 21 + 26 | `0x420c140` |
| 21 + 29 | `0x2020c140` |
| 21 + 30 | `0x4020c140` |
| 21 + 31 | `0x8020c140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per mask, 48 cases total. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits exactly these four pair masks. Their measured
accounting is identical to the proven bit-14 + bit-15 high-bit rule at return
address `0x164b0`: first-order corrections are `-3`/`+2` for mode-bit-6
without countdown and `-15`/`-10` for countdown; bilateral corrections are
`-1`, `-9` and `-26` for countdown/mode joins. Larger multi-high combinations
remain explicit unsupported boundaries.

Reproduction:

```bash
for high_bit in 26 29 30 31; do
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms \
    --state 8 --include-bit8 --extra-bit 6 \
    --extra-bit 14 --extra-bit 15 --extra-bit 21 \
    --extra-bit "$high_bit" --mask 504 --threshold 0 \
    --base out/state8-positive.boundary.vf2snap
done
```

Expected result: `summary: 12/12 exact` for every iteration.
