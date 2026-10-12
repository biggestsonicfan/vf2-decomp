# v0799h - byte-immediate high-byte mask (8 handlers)

## The bug

Byte-size `#imm` operands live in the extension word's LOW byte;
the 68k CPU ignores the high byte. All eight `#imm` print sites
passed the raw word to `#%x`, so any nonzero high byte misreported
the operand (`ori.b #$8e8,(a4)` for bytes `00 14 08 e8`). ROM code
in the walk region carries nonzero high bytes routinely (0x25ec
run: ext words 0x1428/0x0802/0x6600/0x08e8 for byte values
$28/$02/$00/$e8), so this was misreading live code, not a corner.

## Fix

One mask per handler after the immediate read (8 sites: ori/eori,
andi, subi, addi, cmpi, move-ea, move-Dn, movep-abs). Conditions
match each handler's own size rule (`size_code == 0`,
`size_code <= 1`, class-`0x1000`, opcode `0x13fc`). Lengths and
word/long paths untouched.

## Vectors and gates

- Synthetic `imm_byte_high` block: all 8 handlers with ext
  `0x08e8`, each expecting the low byte only; plus the exact ROM
  bytes at 0x265e (`00 16 04 08` -> `ori.b #$8,(a6)`).
- ROM sites (fresh binary): 0x25f8 `ori.b #$e8,(a4)`, 0x265e
  `ori.b #$8,(a6)`, and the 0x25ec run-mates `#$28,-(a0)` /
  `#$2,d1` / `#$0,d7`.
- Stash-gate: old decoder + new vectors FAILs (0%),
  fixed green. All pre-existing byte vectors are canonical
  (zero-high) and unaffected.
- Frontier rescan: zero blanks beyond the 7 known data `.word`s,
  zero `.b #$` prints longer than 2 digits.

## Evidence grades (honest)

0x265e is branch-targeted (exit bra of the 0x25ec run reaches
it); 0x25f8 sits in a 7-instruction contiguous run with a
coherent exit. Both are run-anchored, not entry-proven from a
known function head — the walk interior still lacks entry
mapping (see the walk boundary note). The fix itself does not
depend on the grade: no byte `#imm` print may ever exceed two
hex digits, so the old text was wrong on its face wherever the
high byte was set, code or data.

## Forensics appendix: the 0x265e phantom scare

Two linear windows disagreed at 0x265c-0x265e (move.b vs ori.b).
Byte arbitration: `bra.w` at 0x2658 targets 0x265e exactly, so
0x265c-0x265d are dead fallthrough bytes (`1b 68`) and the
6-byte move.b reading consumed dead bytes + ori opcode + ori
extension. Rule restated: overlapping linear windows are not
two opinions; the branch-target-consistent parse wins, and the
loser is named phantom in the note, not patched. (A related
non-bug: the 0x2650 `move.b a3,d0` line was a correct print of
`(a3)` source with parens misread in haste — checked against
the mode-1 branch, no code touched.)
