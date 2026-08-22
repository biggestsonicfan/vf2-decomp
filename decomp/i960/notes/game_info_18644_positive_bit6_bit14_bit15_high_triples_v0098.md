# `fa_game_info` `0x18644`: positive state-8 bit-14/bit-15 high triples

The remaining three high-bit triples were measured with state bit 8 and
fighter bits 6, 14 and 15:

| High-bit triple | Mask |
| --- | ---: |
| 26 + 29 + 31 | `0xa400c140` |
| 26 + 30 + 31 | `0xc400c140` |
| 29 + 30 + 31 | `0xe000c140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per triple. Every case matched the ROM
at the scheduler boundary, including CPU state, condition state, mutable memory
and instruction/call/return counters.

The native recovery admits exactly these three additional triple masks. Their
measured accounting is identical to the proven bit-14 + bit-15 high-bit rule at
return address `0x164b0`: first-order corrections are `-3`/`+2` for mode-bit-6
without countdown and `-15`/`-10` for countdown; bilateral corrections are
`-1`, `-9` and `-26` for countdown/mode joins. Other triple-high and larger
combinations remain explicit unsupported boundaries.

Reproduction, repeated once for each listed high-bit triple:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms \
  --state 8 --include-bit8 --extra-bit 6 \
  --extra-bit 14 --extra-bit 15 \
  --extra-bit 26 --extra-bit 29 --extra-bit 31 \
  --mask 1016 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

Expected result for each triple: `summary: 12/12 exact`.
