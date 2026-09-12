# v0305: midbody second-contact `g0=1` with compact `0x225cc`

## Verdict

`vf2_hybrid_coli_midbody_tail_execute` now admits the measured path
where the **first** contact returns `g0 = 0` and the **second**
returns `g0 = 1`. The tail takes `cmpobe 0,r6 → 0x22290` and calls
the compact `0x225cc` bit-3 sibling **without** restoring the
fighter assignment (`g7 = fighter1`, `g8 = fighter0`).

Measured total **130** instructions / **5** calls / **6** returns
(129 before the parent ret).

Both-non-zero cascade at `0x22244` remains fail-closed.

## Drive

From `out/coli-midbody-22210.vf2snap`:

```text
vf2probe --snapshot out/coli-midbody-22210.vf2snap \
  --set-u32 0x00512984=0x0 \
  --set-u32 0x00512b24=0x102 \
  --set-u8  0x005131a0=0x01 \
  --set-u32 0x005149cc=0xffff \
  --set-u32 0x00510b24=0x8 \
  --until 0x00022294 --max-steps 5000 --trace
```

| Mutation | Effect |
|----------|--------|
| fighter1 `+0x4 = 0` | force slot-0 one-hit scan (live value is 1) |
| fighter1 `+0x1a4 = 0x102` | bits 8+1: first `0x22298` body 7; second contact `g0=1` |
| fighter1 `+0x820 = 1` | ROM mask table index 1 (=8) |
| `g13+0x40[3] = 0xffff` | non-empty after `andnot` |
| fighter0 `+0x1a4 = 0x8` | compact `0x225cc` bit-3 (g8 is fighter0 here) |

`run_instructions = 129` until IP `0x22294`.

## Accounting

```text
parent shell     7 mov + 2 cmpobe + 5 calls = 14
0x22298 bits8+1  7+ret = 8
0x22298 warm     6+ret = 7
0x22404 warm    13+ret = 14
0x22404 g0=1    72+ret = 73
0x225cc compact 12+ret = 13
total before parent ret = 129
complete_procedure adds parent ret → 130 / 5 / 6
```

Final `g7 = fighter1`, `g8 = fighter0` (no-restore).

## Code notes

Second-contact `coli_22404_body` now receives CPU `g7/g8` as the
swapped pair before the call. The hit path reads exclude
(`g8+0x6dc`) and stores the scan result (`g8+0x6d4`) through `g8`;
warm bit-8-clear never touches those fields, so the previous
unplanted `g8` was invisible until this sibling.

## Fail-closed siblings

- both contacts non-zero (`0x22244` cascade over `+0x804/+0x822/+0x808`)
- slot-1 scan in either contact
- long `0x225cc` body / `g8+0x19f==22` / `g7+0x821!=0`

## Proof

- unit test `test_coli_midbody_second_contact_bit3`: `130/5/6`,
  counter net zero on fighter1, no-restore `g7`/`g8`, both-non-zero
  fails closed
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
