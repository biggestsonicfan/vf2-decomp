# v0312: coli table-walk helper `0x1ab34`

## Verdict

`0x1ab34` walks a type-tagged record chain:

```text
index = g0 & 0x1fff
g0 = *(u32*)(0x0200d34c + index*4) + 8
loop:
    type = *(u8*)g0
    if type == g1 → return g0
    if type == 0 or type == 8 → return 0
    g0 += *(u8*)(0x0001b7f6 + type)
```

Measured two-iteration miss (type 3, size `0x0e`, then type 8):
body **16**, `g0 = 0`. First-iteration match: body **6**, `g0 =
record+8`.

## Drive

From the long-`0x225cc` path (park `out/v310/at-1ab34.vf2snap`):

```text
table[0x421] @ 0x0200d34c → 0x0201acb3
type @ 0x0201acbb = 3
size ROM[0x1b7f9] = 0x0e
type @ 0x0201acc9 = 8  → miss
```

## Proof

- unit test `test_coli_1ab34_walk`: miss `17/0/1` `g0=0`; match
  `7/0/1` `g0=record+8`
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
