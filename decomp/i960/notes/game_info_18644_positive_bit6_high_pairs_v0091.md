# `fa_game_info` `0x18644`: positive state-8 bit-6 high-bit pairs

The already-admitted positive-threshold corridor was measured for all ten
pairwise combinations of high bits 21, 26, 29, 30 and 31, with state bit 8 and
fighter flag bit 6:

| High-bit pair | Mask |
| --- | ---: |
| 21 + 26 | `0x4200140` |
| 21 + 29 | `0x20200140` |
| 21 + 30 | `0x40200140` |
| 21 + 31 | `0x80200140` |
| 26 + 29 | `0x24000140` |
| 26 + 30 | `0x44000140` |
| 26 + 31 | `0x84000140` |
| 29 + 30 | `0x60000140` |
| 29 + 31 | `0xa0000140` |
| 30 + 31 | `0xc0000140` |

Every mask matched the ROM across the three physical distributions, countdown
0/1 and mode-byte bit 6 clear/set: 12 cases per mask, 120 cases total. The
26+29 matrix was already included in the preceding aggregate evidence, so
nine matrices add 108 unique fixtures to the positive family. No C change was
needed; the measurements confirm the existing native corridor for this
pairwise family. Other positive bit-6/high-bit compositions remain unproven
or explicit boundaries.

Reproduction:

```bash
for a in 21 26 29 30 31; do
  for b in 21 26 29 30 31; do
    [ "$a" -ge "$b" ] && continue
    python3 decomp/i960/tools/validate_game_info_state4.py \
      ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
      --extra-bit 6 --extra-bit "$a" --extra-bit "$b" --mask 120 \
      --threshold 0 --base out/state8-positive.boundary.vf2snap
  done
done
```

Expected result: `summary: 12/12 exact` for every one of the ten iterations.
