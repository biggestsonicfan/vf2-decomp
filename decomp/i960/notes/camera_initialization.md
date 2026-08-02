# Camera initialization evidence

The first `fa_camera` dispatch enters at `0x0001d320` with runtime record
`0x00515400`. v0.0.15 accepts the initialization prefix through continuation
`0x0001d458`; the separately validated recurring prefix continues through
`0x0001d660`; the observed post-update gate then reaches fast return `0x0001e524`. The input-bit-3 viewport block is independently recovered and differentially validated through `0x0001d8e8`.

## Recovered effects

- writes the initial camera vectors and tuning constants into the task record;
- initializes global camera scale values at `0x00501084` and `0x00501088`;
- clears camera mode/state fields and installs continuation `0x0001d458`;
- executes the palette averaging helper at `0x000216b8`;
- transforms 125 indexed palette entries from the main-data tables at
  `0x02100800` and `0x02100000` into palette RAM beginning at `0x01802000`;
- executes the observed branch of the state-reset helper at `0x0001f148`.

The alternate `0x0001f148` branch, taken when fighter flag bit 7 is set, calls
`0x0001f24c` twice and is intentionally unsupported until separately captured.

## First-dispatch call sequence

```text
0x0001d444 -> 0x000216b8
0x0001d448 -> 0x0001f148
0x0001d4d4 -> 0x0001f148 (indirect)
0x0001d5cc -> 0x000214dc
0x0001d5d4 -> 0x00020558
0x0001d650 -> 0x0001fc00
```

The first two calls are included in the accepted C initialization prefix. The
remaining four calls are covered by the separately validated recurring prefix
through `0x0001d660`; the post-update gate and input-bit-3 viewport construction path are covered by separate differential boundaries.
