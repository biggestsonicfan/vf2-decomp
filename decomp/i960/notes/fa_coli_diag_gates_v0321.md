# v0321: coli diagnostic-arm gates (bit 18 / scanbyte / r11 / b1b1)

## Verdict

Opens the remaining fail-closed gates on the `r11 != 0` diagnostic
arm and corrects the `g7+0x820` table select.

| shape | total | notes |
|-------|-------|-------|
| warm (bit18 clear, scan 0, r11&lt;20, b1b1≠9) | 302 | unchanged |
| `g7+0x1a4` bit 18 set | 247 | `bbs 18` taken → `0x22c84` |
| r11=20 (20≤r11&lt;40) | 305 | r4=4, `cmpobg 40` taken |
| `g8+0x1b1==9` | 354 | second pair, const `0x9e2c7f` |
| `g7+0x823=1`, two table entries | 353 | loop pairs, no main pair |

## Table select fix

`g7+0x820` in `{5,6}` selects ROM `0x230c8[r4]`; otherwise
`0x230bc[r4]`. v0319/v0320 always read `0x230c8`. The probe snapshot
has `+0x820=1`, so the measured g0 is `0x230bc` (`0x009e017f`).

## Scanbyte walk

`g7+0x823 != 0` loads `main_data[0x0201e880 + scan-1]` once as the
inner table base, then loops `base[r4]` until a zero entry. Zero
jumps to `0x22c84` — **no** main pair after the loop.

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=<r11> --set-u32 0x0050a0b4=<lim> \
  --until 0x000230b8 --max-steps 500
```

Bit 18: `--set-u32 0x00510b24=0x40100`.
Scanbyte: `--set-u8 0x005111a3=1` (game main_data tables).
b1b1: `--set-u8 0x00512b31=9`.
r11=20: `--set-u8 0x005111a2=20 --set-u32 0x0050a0b4=20`.

## Proof

- unit `test_coli_225cc_long`: 302/247/305/354/353
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- r11≥40 (also hits `cmpoble 30` cascade gate)
- `0x43888` bit-20 non-match
- remaining long-body flag siblings
