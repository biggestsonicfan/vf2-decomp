# v0331: coli long-body r11>=40 diagnostic-arm shape

## Verdict

`r11>=40` is already native via the v0321 diagnostic-arm r4-offset
(`cmpobg 20` + `cmpobg 40` → r4=8) and the v0327 `cmpoble 30` cascade
gate (joins at `0x22e24`). This slice adds the missing unit-test
shape.

Unit shape **300**. Diagnostic slot `0x504078` must be cleared so
`0x439ac` does not early-out on a leftover match.

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=40 --set-u32 0x0050a0b4=40 \
  --until 0x000230b8 --max-steps 800
```

Probe measured **300**. Unit 300 including the completed ret.

## ROM tables

r4=8 selects `0x230bc[2]` / `0x230c8[2]` (offsets `0x230d0` /
`0x230c4`). Both planted as `0x1234`.

## Proof

- unit `test_coli_225cc_long`: r11=40 **300**, `0x504078 = 0x1234`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit 4 (4 sites, see v0328 measurement)
- bit-13 profundo
