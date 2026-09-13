# v0339: coli `0x22d8c` g0=5 path (bbs-11 edge)

## Verdict

The `0x22e24` join-block `bbs 11 taken` edge to `0x22d8c` is native at
all four C sites (they mirror the single ROM block). `mov 5, g0` +
`call 0x230d4` takes the previously fail-closed g0=5 fork
(`coli_230d4_long_body` rejected `g0_in == 5` since v0314).

## Measured facts (reference `vf2probe`, `out/v0339/`)

- Entry: `coli-225cc-entry` + `f0+0x1a4 = 0x400100` (bit 22),
  `f1+0x1a4 = 0x800` (bit 11, bit 4 clear).
- `0x22e24 ld f0+0x1a4; bbc 22 nt; ld f1+0x1a4; bbs 4 nt;
  bbs 11 taken → 0x22d8c` (`b22b11-trace.jsonl`).
- `0x22d8c ld; bbc 22 nt; mov 5, g0; call 0x230d4` (disasm confirms
  `mov 5, g0`; `0x22d90 bbc 22 → 0x22e24` taken edge unmeasured).
- g0=5 fork: `0x23138 cmpobne 5 nt`, `r3 = 31+9 = 40`,
  `0x23144 bbc 11 nt` (bit set → fall through; taken/r3=40 edge
  unmeasured), `r3 = 42`, `b 0x231ec`.
- `0x231ec` tail: `bbc 25 taken` (set edge → `0x231f4` table load,
  unmeasured), branch byte bit 6 taken (set edge unmeasured),
  `g0 = *(0x0201cd74 + 42*4) = *(0x201ce1c) = 0xeb`,
  `call 0x23238` early-out (`0xeb != 0x2ce`: lda/cmpobne/ret).
- Return tail `0x22d9c`: `g8+0x198 = g0 + 0x0c010000 = 0x0c0100eb`;
  `r14 = ((r11 >> 1) - 7) * 2` uses the CALLER (0x225cc-frame) r11 —
  the 0x230d4-frame table base does not survive its ret.
  On the drive r11 = 0 (scanbit miss pack) → stos `0xfff2`.
  `bbc 20 taken` (shli edge unmeasured).
- `0x23070`: `ldis 0x50028 >= 0` skips `call 0x18a54`
  (call edge unmeasured), `b 0x230b8`, ret to `0x22294`.
- Full path entry→`0x22294`: 113 steps to `0x230d4` + 36 tail = 149.

## Native (`coli_22d8c_g05_tail`, helper body 40 excl. frame ret)

Shared helper takes `(g7, g8, r11_in)`; re-reads both flag words;
fail-closes every unmeasured edge (bbc-22 taken, bbc-11 taken,
bbs-25 set, branch bit 6 set, bbc-20 nt, `0x50028 < 0`).
The four C sites (bit-14 path, r11>=30 path, +0x1ac path,
0x22e24-join path) call it and return from the long body —
the ROM path returns from `0x225cc` via `0x230b8` and never rejoins.
Wrapper still reports 4/4 calls/rets (sibling convention; true shape
is 2 calls / 3 rets — see below).

## Proof

- unit `test_coli_225cc_long` v0339 shape: 149, `f1+0x198 =
  0x0c0100eb`, `f0+0x1234 = 1`, `f1+0x6d8 = 1`, `f1+0x5de = 0xfff2`.
- PUNCH 320/320 MATCH (14,962,620 insns).
- input-17 64/64 MATCH (2,428,988 insns).
- ctest Debug 56/56.

## Still fail-closed

- `0x22c88 bbs 16 → 0x22d8c` (post-diagnostic bit-16 edge). Drive
  recipe for a follow-up: `g8+0x1a4` bit 16 + `g7+0x821 = 4`
  (scan 4 skips both `0x230a0` early exits) → warm cascade →
  `0x22c84` → `0x22d8c`. Then bit 22 clear rejoins warm `mov 4`,
  bit 22 set takes this slice's g0=5 helper.
- `0x227dc` (needs type-5 record).
- `0x502a4` (board bit 9 clear on bit-14).
- g0=5 fork siblings: r3=40 (bit-11-clear), `0x231f4` table return
  (bit-25-set), branch-byte-set, bbc-20-nt, `0x18a54` call.
- Call/ret counters on this shape report the wrapper 4/4 convention,
  not the true 2/3. Thread real counters through
  `coli_225cc_long_body` when ROM-backed counter coverage is added.
