# `fa_game_info` `0x18644`: positive state-8 multi-flag high-bit extensions

The measured positive-threshold fighter fields combining state bit 8, fighter
bit 6, one of the four measured compositions of bits 14/15/16, and one of
high bits 26, 29, 30 or 31 were recovered at the `0x18644` child:

| Fighter composition | High bit 26 | High bit 29 | High bit 30 | High bit 31 |
| --- | ---: | ---: | ---: | ---: |
| 14 + 15 | `0x400c140` | `0x2000c140` | `0x4000c140` | `0x8000c140` |
| 14 + 16 | `0x4014140` | `0x20014140` | `0x40014140` | `0x80014140` |
| 15 + 16 | `0x4018140` | `0x20018140` | `0x40018140` | `0x80018140` |
| 14 + 15 + 16 | `0x401c140` | `0x2001c140` | `0x4001c140` | `0x8001c140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per mask, 192 cases total. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits only one of the four measured high bits for each
fighter composition. The measured instruction accounting is composition
specific and is applied only at return address `0x164b0`:

* bits 14 + 15: first-order corrections are `-3`/`+2` for mode-bit-6 without
  countdown and `-15`/`-10` for countdown; bilateral corrections are `-1`,
  `-9` and `-26` for countdown/mode joins.
* bits 14 + 16: first-order corrections are `-1` and `+4` for mode-bit-6
  without countdown; bilateral corrections are `-1`, `-6` and `-1`.
* bits 15 + 16: first-order corrections are `-2`/`-2` for mode-bit-6 without
  countdown and `-15`/`-10` for countdown; bilateral corrections are `-1`,
  `-13` and `-26`.
* bits 14 + 15 + 16: first-order corrections are `-1`/`+4` for mode-bit-6
  without countdown and `-15`/`-10` for countdown; bilateral corrections are
  `-1`, `-7` and `-26`.

Multiple high bits in any of these cross-family compositions remain explicit
unsupported boundaries.

Reproduction for each composition and high bit:

```bash
for high_bit in 26 29 30 31; do
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms \
    --state 8 --include-bit8 --extra-bit 6 \
    --extra-bit 14 --extra-bit 15 --extra-bit "$high_bit" \
    --mask 248 --threshold 0 --base out/state8-positive.boundary.vf2snap
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms \
    --state 8 --include-bit8 --extra-bit 6 \
    --extra-bit 14 --extra-bit 16 --extra-bit "$high_bit" \
    --mask 248 --threshold 0 --base out/state8-positive.boundary.vf2snap
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms \
    --state 8 --include-bit8 --extra-bit 6 \
    --extra-bit 15 --extra-bit 16 --extra-bit "$high_bit" \
    --mask 248 --threshold 0 --base out/state8-positive.boundary.vf2snap
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms \
    --state 8 --include-bit8 --extra-bit 6 \
    --extra-bit 14 --extra-bit 15 --extra-bit 16 \
    --extra-bit "$high_bit" --mask 504 --threshold 0 \
    --base out/state8-positive.boundary.vf2snap
done
```

Expected result: `summary: 12/12 exact` for every iteration.
