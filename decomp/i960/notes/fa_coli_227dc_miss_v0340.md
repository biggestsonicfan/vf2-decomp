# v0340: coli `0x227dc` miss path (bit-13 + bit-3 + `0x844` bit 30)

## Verdict

`0x227dc` is native for the measured miss shape: `g8+0x1a4` bits
13+3, scan 1, `g7+0x844` bit 30 set, `g7+0x848` index with a clean
type-8-ending chain. Straight line: `ld/st/mov 5/call 0x1ab34`,
`ldos/shlo/addi/st`, `ldib/stib`, ret from `0x225cc` (unit **87**).
A type-5 match stays fail-closed (unmeasured).

## Measured facts (reference `vf2probe`, `out/v0340/`)

- Reach `0x227dc` in 52 steps from `coli-225cc-entry` with
  `f1+0x1a4 = 0x2008`, `f0+0x844 = 0x40000000`, scan 1.
  Scan 0 takes the v0323 early exit instead; `0x5b8` bit 0 is
  already clear live.
- Forcing `0x844` bit 30 with `0x848 = 0` FAULTS the reference
  inside `0x1ab34` (`table[0] = 0x09090000`, `ldob` at
  `0x09090008` out of bounds). No live snapshot has `0x844` bit 30
  set, so only synthetic `(bit30 + walkable index)` states complete.
- With `0x848 = 1`: `table[1] = 0x02014d6d`, chain `02 → 04 →
  `08` (steps `0x1b7f8 = 0x07`, `0x1b7fa = 0x11`), 24-step walk,
  miss (`g0 = 0`).
- Tail with `g0 = 0`: `ldos 0x1(0) = 0`, `g7+0x198 =
  0x11000000`, `ldib 0x3(0) = 0`, `g7+0x822 = 0`;
  `g8+0x198 = 0x848 = 1`. Full path entry→`0x22804` is 86 steps.

## Native

- Inline at the `gate844` bit-30 site: reuses `coli_1ab34_body`
  (type 5), admits only `walk == 0`, then the generic tail.
  Match (`walk != 0`) fails closed.
- Wrapper (`coli_225cc_body`): scan==1 admitted ONLY for
  bit13+bit3, no bits 15/16, `0x844` bit 30 set, routing bit-3-set
  into long_body. All other scan!=0 still fail closed; scan==0
  paths byte-identical to before.
- `coli_225cc_long_body` early bit-3 gate made scan-aware: scan==0
  keeps the proven `+4`; scan==1 without bits 15/16 continues with
  `+3` (cmpibne skips the bbs-3 exit); all else fail-closed.
  (`scan821` hoisted to function scope for this.)

## Important adjacent finding

The wrapper rejected scan!=0 since v0304, so every scan!=0
long-body path implemented in v0334/v0336 (scan 1/2/5/6 shapes,
0x227ac scale, 0x227c4 with scan!=0/4) was unreachable dead code
— probe-measured but never executed natively. This slice opens
exactly one measured scan-1 combination; the rest stays gated.
Verifying the v0336 scan-1 siblings (bit-30-clear 0x22744 path
with unit, scan guard on the 0x227c4 branch) is queued work.

## Proof

- unit `test_coli_225cc_long` v0340 shape: 87, `f1+0x198 = 1`,
  `f0+0x198 = 0x11000000`, `f0+0x1234 = 1`.
- PUNCH 320/320 MATCH, input-17 64/64 MATCH, ctest Debug 56/56.

## Still fail-closed

- `0x227dc` type-5 match; `0x848 = 0` (reference faults).
- `0x22c88` bbs-16 edge (drive: scan 4 + bit 16).
- `0x502a4` (board bit 9 clear on bit-14).
- All other scan!=0 entries (v0336 siblings pending).
