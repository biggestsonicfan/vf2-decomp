# v0328 (measurement): coli g8+0x1a4 bit 4 — 4 sites

## Measured

Probe from `out/coli-225cc-entry.vf2snap` with `--set-u32 0x00512b24=0x10`:
**301** instructions. Four sites fire:

| Site | Address | Action |
|------|---------|--------|
| Cascade | `0x22b7c` | `flags_g8 & 0x4010 == 16` → `r11*3>>1`, `g0=0x23d6b`, skip `g8+0x804` |
| Post-diag | `0x22c98` | `bbs 4 taken` → `0x22e24` join |
| After `0x230d4` | `0x22e44` | `bbc 4 nt` → `lda 0x2ce, g0` before `0x23238` |
| Miss tail | `0x22ee0` | `bbs 4 taken` → offsets `0x1c`/`0x10` instead of `0x18`/`0x8` |

## Not recovered

An implementation covering all four sites still fail-closed in the
unit test (`st=8`) despite the probe completing at 301. The cascade
site (`0x22b7c`) and the diagnostic interact in a way that needs a
dedicated trace; kept fail-closed.

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x10 \
  --until 0x000230b8 --max-steps 500 --memory-trace
```
