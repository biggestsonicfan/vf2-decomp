# v0320: coli diagnostic cascade `0x439ac`/`0x43888` completion

## Verdict

Completes the fail-closed edges opened in v0319 and fixes incorrect
effects in the v0319 `0x43888` stub.

### `0x439ac` (count table)

| shape | body (excl ret) | total vs warm 302 |
|-------|-----------------|-------------------|
| count=0, no match | 13 | 302 |
| count=1, no match | 17 | 306 |
| count=3, no match | 25 | 314 |
| count>=4 early-out | 2 | 291 |
| count=0, match table[1] | 6 | 295 |
| count=3, match table[2] | 14 | 303 |

The scan starts at index `count+1` and walks down to 1. `bl` after
`cmpdeco` is taken while the pre-decrement index is greater than 1.
`cmpoble 4,r3` branches when `count >= 4` (not `>`).

### `0x43888` (event queue)

v0319 wrote a single `2` to `0xe80004` and skipped the ring. Measured
warm path:

1. write `33` (`0x21`) twice to `0xe80004`
2. if byte `0x504001 < 16`: increment it, store `g0` at
   `0x504020[slot]`, `slot = (slot+1)&15`
3. write `0x421` twice to `0xe80004`

| shape | body (excl ret) | total |
|-------|-----------------|-------|
| warm (gate&12==0, bit20 clear) | 26 | 302 |
| gate&12!=0, branch-byte bit0 set | 9 | 285 |
| gate&12!=0, bit0 clear | 29 | 305 |
| bit20 set + `(g0&0x00ff0000)==0x009e0000` | 32 | 309 |

`g0==0xae101f` jumps to the store path (static from disassembly; unit
shape uses synthetic ROM). Branch-byte is `ldob *(0x50016c + 0x3351)`.
Bit-20 match does `g0 -= 0x20000` (`shlo 17,1` / `subo`). Non-match
(`cmpobne` taken) remains fail-closed — ROM `0x230c8` cannot be
patched via `vf2_model2a_write_u32` (ROM writes are ignored).

### Later `bbc 20` at `0x22e74`

Setting `0x500068` bit 20 also affects a second `bbc 20` in the long
body tail. Taken → `shli 1,r3,r14` before the `stos` to `g8+0x5de`
(+1). Implemented; combined with the `0x43888` subtract this is the
measured 309.

## Drive

From `out/coli-225cc-entry.vf2snap`, `--set-u8 0x005111a2=1`
(`g7+0x822`) and `--set-u32 0x0050a0b4=2` (lim >= r11):

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --until 0x000230b8 --max-steps 500 --memory-trace
```

Sibling mutations: `0x50406a` (count), `0x504078` (table[1]),
`0x50002c` (gate), `0x59c351` (branch byte), `0x500068` (runtime).

## Proof

- unit `test_coli_225cc_long`: shapes 302/291/295/306/285/305/309
- warm 302 checks `0x50406a==1`, `0x504078==0x1234`, `0xe80004==0x421`,
  ring `0x504001==1`, `0x504020==0x1234`
- bit20-match checks ring `0x504020==0x009c1234`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- `0x43888` bit-20 non-match (`cmpobne` taken)
- `g0==0xae101f` unit shape (static recovery; needs a ROM-patched
  unit drive if a differential pin is required)
- `g7+0x1a4` bit 18 set, `g7+0x823 != 0`, `r11 >= 20` on the
  diagnostic arm
- deep bit-13 arms, remaining long-body flag siblings
