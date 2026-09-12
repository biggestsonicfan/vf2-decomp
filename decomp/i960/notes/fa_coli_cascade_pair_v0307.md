# v0307: coli cascade pair-greater arm (`cmpobg` → `0x22290`)

## Verdict

When both contacts hit and both `+0x804` bit 15 are clear, the
cascade reaches the pair compare at `0x2225c`. The **only** extra
reliable arm is `cmpobg` taken when `fighter1+0x822 > fighter0+0x822`,
which jumps to the same swapped `0x22290` single compact `0x225cc`
as the v0306 bit-15 early-out.

Measured **258/5/6** (257 before the parent ret).

## Condition-code note

The subsequent `bl`/`bg` arms at `0x22268`/`0x22278` do **not**
inherit the pair/range compare in the reference executor. Measured
drives with `f1+0x822 < f0+0x822` or equal-pair/range-greater both
fell through to the `0x2227c` tie-break instead of taking `bl`/`bg`.
Those arms and the tie-break (double `0x225cc`, second is the long
body) remain fail-closed.

## Drive

```text
# both bit15 clear, f1+0x822=5 > f0+0x822=3
--set-u32 0x00513184=0x0
--set-u32 0x005131a2=0x5
--set-u32 0x005111a2=0x3
--until 0x00022290 --max-steps 8000
# 243 instructions to the call
```

## Accounting

```text
parent  7 mov + 2 cmpobe + 2 ld + 2 bbc/bbs + 2 ldos + 1 cmpobg
        + 5 calls = 21
children 8+8+73+134 = 223
0x225cc compact 12+ret = 13
total before parent ret = 257
complete_procedure → 258 / 5 / 6
```

## Proof

- unit test pair-greater `258/5/6`; bit15-clear both with equal
  pair fails closed
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
