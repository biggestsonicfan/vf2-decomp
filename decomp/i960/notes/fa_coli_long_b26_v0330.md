# v0330: coli long-body g8+0x1a4 bit 26

## Verdict

`g8+0x1a4` bit 26 set at the post-diagnostic cascade (`0x22c8c`) is
native. `bbs 26 taken` joins at `0x22e24` (same join as bit 14 and
`r11>=30`), skipping the `g7+0x828` cascade entirely.

The subsequent `0x230d4` call takes the **compact** path (v0311)
instead of the long path (v0314). Compact body 15, `g0` from
main-data `0x0201cc54` (when `+0x142` bit 15 clear).

Unit shape **263** with `g8+0x198 = 0x0c004421`.

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x04000000 \
  --until 0x000230b8 --max-steps 800
```

Probe measured **263**. Unit 263 including the completed ret.

## Unit-test notes

- Needs main-data `0x1cc54 = 0x4421` (compact table slot).
- Needs `0x1ab34` index `0x421` (from `g0=0x4421`) with a two-iteration
  type-3/8 miss record.
- Diagnostic slot `0x504078` must be cleared so `0x439ac` does not
  early-out on a leftover match from a prior shape.

## Proof

- unit `test_coli_225cc_long`: bit26 **263**, `g8+0x198 = 0x0c004421`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit 4 (4 sites, see v0328 measurement)
- `r11>=40` diagnostic-arm r4-offset
- bit-13 profundo
