# v0317: coli `0x18b58` bit-2-set FIFO path

## Verdict

`0x18b58` is now fully native on the measured shapes:

| Condition | body |
|-----------|------|
| `g7` word bit 2 clear | 2 (early-out) |
| bit 2 set, `g7+0x840` bit 0 clear | 29 (FIFO delta + tail) |
| bit 2 set, bit 0 set | 13 (skip FIFO + tail) |

FIFO command `0x2d805b5b` writes `g7+0x26` (halfword), `g7+0x80`,
`g7+0x88`, reads two words (buffer model: last write). Float `subr`
into `g7+0x18` / `g7+0x20`. Common tail: `g7+0x84 = g7+0x1c -
g7+0x1f8` (float), clear `g7` word bit 2, clear `g7+0x1a4` bit 4.

## Drive

From `out/coli-225cc-entry.vf2snap`, type 22, index 1, `g7` word = 4.

## Proof

- unit test type22 bit-2-clear 53/3/4; bit-2-set **80/3/4**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug
