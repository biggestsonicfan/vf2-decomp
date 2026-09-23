# v0343: coli bit-30-clear scan-1 leaves (0x22744 family)

## Verdict

Recovered. All three `+0x828` leaves of the bit-30-clear path are
native with units, and executing the previously-dead v0335/v0336/v0337
code exposed and fixed four latent bugs plus one miscount.

## Probes (all from `out/coli-225cc-entry.vf2snap`, bit13 + bit 3 +
scan 1, `g7+0x844` bit 30 clear, entry → live return `0x22294`)

- L1 (`+0x828 = 0`): `0x22778` → scan re-read → `0x227ac` scale
  (`r11 -= r11>>2`, `r9 *= 0.75`) → cascade `0x22914` → warm join.
  **275 steps**. `f1+0x198 = 0x0c0004ac`, `f0+0x194 = 0x14000001`,
  `f1+0x5e4 = 0xc8a3d709`, `+0x5de = 0xfff2`.
  (`out/v0342/L1-full.jsonl`)
- L3 (`+0x828 = 0x1000`, bit 12): `0x22794` scale (`r11 >>= 1`,
  `r9 *= 0.5`) → cascade at `0x22918` (skips the `mov 2,r8`).
  **271 steps**. Same integers as L1, `f1+0x5e4 = 0xc85a740c`.
  (`out/v0342/L3-full.jsonl`)
- L2 (`+0x828 = 0x2000`, bit 13): `0x227c4` diagnostic pair
  (`g0 = 0x9e167f`, then 1) → alt tail at `0x22848` with `g0 = 1`
  into the `0x230d4` call. **224 steps**. `f1+0x198 = 0x0e000104`
  (`0x104 + 7<<25`), `f0+0x194 = 0x14000002`,
  `f1+0x5e4 = 0xc8da740c`, `+0x5de = 0x0006`.
  (`out/v0342/L2-full.jsonl`)

Any other `+0x828` value reduces to one of these leaves (only bits
12/13 are tested downstream), so the four shapes (v0340 + L1/L2/L3)
cover the whole admitted gate.

## Code changes (`src/recovered/hybrid.c`)

Wrapper: scan==1 + bit13 + no bits 15/16 now enters `long_body`
without the `0x844`-bit-30 condition (bit 30 is branched on
inside, v0340 site vs `0x22744` path). The wrapper `0x844` read is
gone; `long_body` already re-reads it.

Bugs found by executing the dead code (all previously unverified,
warm shapes never checked these values):

1. `0x230d4` bit-3 fallthrough at `0x23190` adds 4 to `r9`
   (`0x23194 addo`); was `UNSUPPORTED`. All three leaves carry
   bit 3.
2. `0x22778` scan==1 sub-branch: ROM re-reads scan at `0x22788`
   (`ldob` + `cmpobne` + `b 0x227ac`); the model missed the re-read
   and the branch (was short by 2).
3. `bit13_alt_join` hardcoded `g0 = 0` into the `0x230d4` call;
   `0x227c4` arrives with `g0 = 1` (wrong table slot → unmapped
   read). Now threads the incoming `g0` (profundo `0` unchanged).
4. `0x231c0` is `addo r5,r9,r9` (`r9 += r5`), not `r9 = r5`.
   Identical when `r9` enters 0 (all bit-3-clear shapes); destroyed
   the bit-3 `+4` otherwise. L1/L3 briefly passed via the wrong
   slot 34 holding the same `0x4ac` as slot 38 by luck.
5. Float-tail `divr r4,r9,r4` computes `r9/r4` (dst = src2/src1),
   not `r4/r9`. Warm never checked floats, so the inversion hid
   until L1/L3 (verified bit-exact: `0xc8a3d709`, `0xc85a740c`).
6. `0x227c4` branch missed `b 0x22848` (+1).

## Test notes

- `test_coli_225cc_long` L1/L3/L2 shapes (275/271/224) with full
  live-state setups, including: live `0x4ac`-slot chain
  (`0x0200ee55`, `03`/`08`), slot 38 = `0x4ac`, L2 slot 14 =
  `0x104`, ROM `0x23282 = 0x06`, `0x50a14c = 0x16`,
  diag-slot `[0x504078] = 0` (prior diag-pair shapes pollute it
  with their `g0`, causing an early match and a 7-step
  undercount — same residue class as the `0x50a0b4`/`0x50a16e`
  resets).
- Debug `fprintf` checkpoints were used during the hunt and fully
  removed (no `stdio.h`, no prints in the final tree).

## Proof

- units L1/L2/L3: **275/271/224** exact, all stores exact.
- PUNCH **320/320 MATCH**, input-17 **64/64 MATCH**.
- `ctest -C Debug`: **56/56**.

## Still fail-closed

- `0x227dc` type-5 match; `0x848 = 0` (reference faults).
- `0x22c88` bbs-16 edge; `0x502a4` (needs `dmovt`).
- scan 2/5/6 shapes (implemented, still gated), bit15/16+scan1
  early exits, all other scan!=0 combinations.
