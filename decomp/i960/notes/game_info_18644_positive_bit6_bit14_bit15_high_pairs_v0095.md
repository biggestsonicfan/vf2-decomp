# `fa_game_info` `0x18644`: positive state-8 bit-14/bit-15 high-bit pairs

The six pairwise combinations of high bits 26, 29, 30 and 31 were measured
with state bit 8 and fighter bits 6, 14 and 15:

| High-bit pair | Mask |
| --- | ---: |
| 26 + 29 | `0x2400c140` |
| 26 + 30 | `0x4400c140` |
| 26 + 31 | `0x8400c140` |
| 29 + 30 | `0x6000c140` |
| 29 + 31 | `0xa000c140` |
| 30 + 31 | `0xc000c140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per mask, 72 cases total. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits exactly these six pair masks. Their measured
accounting is identical to the proven single-high bit-14 + bit-15 rule at
return address `0x164b0`: first-order corrections are `-3`/`+2` for mode-bit-6
without countdown and `-15`/`-10` for countdown; bilateral corrections are
`-1`, `-9` and `-26` for countdown/mode joins. Pairs involving high bit 21 and
larger multi-high combinations remain explicit unsupported boundaries.

Reproduction:

```bash
for first_high in 26 29 30; do
  for second_high in 29 30 31; do
    if [ "$first_high" -lt "$second_high" ]; then
      python3 decomp/i960/tools/validate_game_info_state4.py \
        ./build/vf2i960 /path/to/vf2-roms \
        --state 8 --include-bit8 --extra-bit 6 \
        --extra-bit 14 --extra-bit 15 \
        --extra-bit "$first_high" --extra-bit "$second_high" \
        --mask 504 --threshold 0 \
        --base out/state8-positive.boundary.vf2snap
    fi
  done
done
```

Expected result: `summary: 12/12 exact` for each of the six pair masks.
