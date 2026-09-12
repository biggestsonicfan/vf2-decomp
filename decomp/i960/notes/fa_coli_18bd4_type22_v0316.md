# v0316: coli type-22 shortcut `0x18bd4`

## Verdict

`coli_225cc_body` now admits `g8+0x19f == 22`. The shortcut replaces
the 248-step long body with:

```text
counter++ / type==22 → call 0x18bd4 → b 0x230b8
```

`0x18bd4` is a two-call leaf:

1. `0x1ab34` type-5 table walk (native v0312)
2. `0x18b58` bit-2-clear early-out (body 2)

First-hit type-5 unit shape completes **53 / 3 / 4**.

## Measured path (reference, 69 steps + ret)

From `out/coli-225cc-entry.vf2snap` with `g8+0x19f=22` and
`g8+0x19c` index 1 (real main-data table):

- `0x1ab34` multi-iter miss (type not 5/0/8 then type 8)
- `0x18b58` bit 2 clear early-out
- `0x18bd4` parent 34 + children, stores at `g7+0x198`, `g7+0x1c`,
  `g8+0x198`, `g7+0x822`, `g8` bit 4 clear, `g7+0x1a4` bit 21 clear,
  `chkbit 10` / `alterbit 6` on fighter words

## Open siblings

- `0x18b58` bit 2 set (FIFO delta path)
- `g8+0x19c` bit 15 set (`notbit 15`)
- `0x1ab34` type-5 miss (`walk == 0`)

## Proof

- unit test `test_coli_225cc_type22`: 53/3/4, packed stores exact
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**
