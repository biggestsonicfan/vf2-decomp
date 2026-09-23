# v0315 Fase 9: taint/layout after coli long body

## Infer structs (`out/v314/225cc-long.jsonl`)

```text
infer_structs.py out/v314/225cc-long.jsonl \
  --base f0=0x00510980 --base f1=0x00512980 --min-count 3
```

| Offset | Bases | R/W | Notes |
|--------|-------|-----|-------|
| `+0x01a4` | **f0,f1** | R17 | bitmask flags; bilateral |
| `+0x0828` | f0 | R5 | type/index byte+half; g7 on this drive |
| `+0x06d8` | f1 | R2 W1 | cursor byte (0→1) |
| `+0x0821` | f0 | R3 | scan byte; must be 0 |

`+0x1a4` is the stable bilateral fighter-flag field. Names stay
`field_XXXX` until independent semantic evidence exists.

## Frontier

Long body now closed on the v0288 drive. Remaining coli frontier:
`0x18bd4` shortcut (DEFER v0291), unmeasured flag-bit siblings in
`0x225cc`/`0x230d4`/cascade, and other `0x22404` mask shapes.

## Pin

No C change in this note. PUNCH / input-17 / ctest already green
from v0314+v0315.
