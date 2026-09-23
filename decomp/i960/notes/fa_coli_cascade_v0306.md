# v0306: coli slot-1 contact scan + both-hit cascade early-out

## Verdict

1. `coli_22404_body` recovers the measured **slot-1** 15-trip
   `bbc`/`setbit` scan (`r4 = 15..1`). One-hit shape body **133**,
   `g0 = 1`, pending bit 1 set.
2. Mid-body tail admits **both contacts hit** when the cascade
   early-outs at `0x22258` (`bbs 15, fighter1+0x804`): swapped
   assignment, single compact `0x225cc`. Measured **255/5/6**
   (254 before the parent ret).

The `0x2227c` double-call tie-break (which invokes the long
`0x225cc` body) and other cascade arms remain fail-closed.

## Drive (cascade early-out)

From `out/coli-midbody-22210.vf2snap`:

```text
vf2probe --snapshot out/coli-midbody-22210.vf2snap \
  --set-u32 0x00510984=0x0 \
  --set-u32 0x00512984=0x1 \
  --set-u32 0x00510b24=0x10a \
  --set-u32 0x00512b24=0x102 \
  --set-u8  0x005111a0=0x01 \
  --set-u8  0x005131a0=0x01 \
  --set-u32 0x005149cc=0xffff \
  --set-u32 0x00510b28=0x0 \
  --set-u32 0x00512b28=0x0 \
  --set-u32 0x00514a0c=0x0 \
  --set-u32 0x00513184=0x8000 \
  --until 0x00022294 --max-steps 8000 --trace
```

| Mutation | Effect |
|----------|--------|
| fighter0 slot 0, bits 8+1+3 | first contact hit slot-0; compact `0x225cc` g8 |
| fighter1 slot 1, bits 8+1 | second contact hit slot-1 (pending bit 1) |
| both `+0x820 = 1`, table[3]=`0xffff` | non-empty mask |
| fighter1 `+0x804 bit 15` | cascade `bbs` at `0x22258` → `0x22290` |

Same-slot hits cannot both succeed: the first sets the shared
pending bit and the second early-outs `g0=0`.

## Slot-1 scan

```text
scanbit mask → r5
mov 15, r4 / mov 0, r7
loop r4 = 15..1:
    ld g13+0x40[r4]
    bbc r5 → skip setbit
    setbit r4, r7
    cmpdeco / bl
or r7, acc
clrbit r5, mask
b scanbit
```

Unlike slot-0 (`acc |= table[scanbit]`), slot-1 collects the
indices `i` whose table word has the scan bit set.

## Accounting

```text
parent     7 mov + 2 cmpobe + 2 ld + 2 bbc/bbs + 5 calls = 18
0x22298    7+ret = 8   (twice)
0x22404    72+ret = 73 (slot-0 hit)
0x22404    133+ret = 134 (slot-1 hit)
0x225cc    12+ret = 13
total before parent ret = 254
complete_procedure → 255 / 5 / 6
```

## Fail-closed

- cascade fall-through (`+0x822` / `+0x808` compare) and the
  `0x2227c` `call 0x22290` tie-break (second long `0x225cc`)
- `g8+0x26 != 0` FIFO cursor
- long `0x225cc` body / `0x18bd4` shortcut

## Proof

- unit tests: slot-1 `134/0/1`; both-hit cascade `255/5/6`;
  tie-break and bit15-clear fail closed
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
