# `fa_game_info` `0x18644`: positive state-8 bit-6 high-bit masks

The existing native positive-threshold corridor was measured for five
isolated high-bit masks with state bit 8 and fighter flag bit 6:

- `0x200140`: bit 21;
- `0x4000140`: bit 26;
- `0x20000140`: bit 29;
- `0x40000140`: bit 30; and
- `0x80000140`: bit 31.

Each mask matched the ROM across the three physical distributions, countdown
0/1 and mode-byte bit 6 clear/set: 12 cases per mask. Two aggregate masks were
also measured: `0x24000140` (bits 26+29) and `0xe4200140` (bits
21+26+29+30+31). Each aggregate also matched all 12 cases. No C change was
needed; these measurements confirm the already-admitted high-bit corridor for
the tested masks.

Reproduction for the isolated masks:

```bash
for bit in 21 26 29 30 31; do
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
    --extra-bit 6 --extra-bit "$bit" --mask 56 --threshold 0 \
    --base out/state8-positive.boundary.vf2snap
done
```

Expected result: `summary: 12/12 exact` for every iteration.

The aggregate reproductions use:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 26 --extra-bit 29 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap

python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 21 --extra-bit 26 --extra-bit 29 \
  --extra-bit 30 --extra-bit 31 --mask 1016 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```
