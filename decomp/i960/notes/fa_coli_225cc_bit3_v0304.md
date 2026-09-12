# v0304: compact `0x225cc` bit-3 sibling + midbody non-zero tail

## Verdict

1. `coli_225cc_body` recovers the measured 12-instruction early-out
   (`g8+0x1a4` bit 3 set): counter++ at `g7+0x1234`, skip `0x18bd4`
   and the long body, counter-- at `0x230a0`, ret.
2. `vf2_hybrid_coli_midbody_tail_execute` now admits the path where
   the first contact returns `g0=1` and the second returns 0, then
   calls the compact `0x225cc` sibling. Measured total **132**
   instructions (131 before the parent ret), **5 calls**, **6 returns**.

The 248-instruction long `0x225cc` body remains fail-closed.

## Drive

From `out/coli-midbody-22210.vf2snap`:

```text
vf2probe --snapshot out/coli-midbody-22210.vf2snap \
  --set-u32 0x00510b24=0x102 \
  --set-u8  0x005111a0=0x01 \
  --set-u32 0x005149cc=0xffff \
  --set-u32 0x00512b24=0x8 \
  --until 0x00022294 --max-steps 5000 --trace
```

| Mutation | Effect |
|----------|--------|
| fighter0 `+0x1a4 = 0x102` | bits 8+1: first contact `g0=1`; second `0x22298` sibling |
| fighter0 `+0x820 = 1` | ROM mask table index 1 (=8) |
| `g13+0x40[3] = 0xffff` | non-empty after `andnot` |
| fighter1 `+0x1a4 = 0x8` | compact `0x225cc` bit-3 exit |

`run_instructions = 131` until IP `0x22294` (parent ret not executed).

## Accounting

```text
parent shell     11 mov/cmpobe + 5 calls = 16
0x22298 warm     6+ret = 7
0x22298 bits8+1  7+ret = 8
0x22404 g0=1     72+ret = 73
0x22404 warm     13+ret = 14
0x225cc compact  12+ret = 13
total before parent ret = 131
complete_procedure adds parent ret → 132 / 5 calls / 6 returns
```

Warm both-zero pin remains **56 / 4 / 5**.

## Fail-closed siblings

- `0x225cc` `g8+0x19f == 22` (`call 0x18bd4`)
- `0x225cc` `g7+0x821 != 0` (long body at `0x22600`)
- `0x225cc` bit 3 clear (248-insn resolver)
- second contact `g0 != 0`
- slot-1 contact scan / `g8+0x26 != 0` FIFO

## Proof

- unit test `test_coli_midbody_contact_hit_bit3`: `132/5/6`, counter
  net zero, fighters restored (not swapped)
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
