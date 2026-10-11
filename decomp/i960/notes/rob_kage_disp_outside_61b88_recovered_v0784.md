# v0784 - 0x61b88 rob_kage_disp_outside recovered

## Scope

Recovered `0x61b88` (24 B / 3 blocks / 6 instr) as a 1-to-1
C translation. **First recovery from the v0783 87 KB gap
slice** (`gap_87kb_characterisation_v0783.md`), using its
shipped-symbol scaffold. No provisional name ever covered
this address, so the shipped name
(`original_symbols.csv` record 289) is adopted as-is —
no rename slice needed.

## Disassembly

```
00061b88  ldob     0x0000002a(g7), r3
00061b8c  cmpobne  0, r3, 0x00061b9c
00061b90  ld       0x00000684(g7), g4
00061b94  mov      2, g5
00061b98  call     0x00061ba0
00061b9c  ret
```

Extent `0x61b88..0x61ba0` confirmed by two independent
sources per the v0734a rule: `vf2i960 function` measures
`end=0x61ba0`, and `vf2i960 disasm` shows `ret` at
`0x61b9c` (= end − 4). Name source is the shipped symbol
table (third signal).

## Paths

- **Path A** (byte at `g7+0x2a != 0`): `cmpobne` taken,
  straight to `ret`. Fully admitted: `VF2_OK`,
  `ip=0x61ba0`, `executed += 2`, no persistent write
  (the `ldob`'s `r3` is frame-local and evaporates).
- **Path B** (byte `== 0`): fall through, `g4 = *(g7+0x684)`,
  `g5 = 2`, then **REFUSE** the `call 0x61ba0` at this
  function's own ret slot (`ip=0x61b9c`, `executed += 4`,
  `VF2_ERROR_UNSUPPORTED`).

The `ne` condition is symmetric, so the disassembler's
operand order for `cmpobne` is immaterial — taken iff the
byte is nonzero. (No repeat of the v0732 asymmetric-operand
trap.) The v0783 note independently confirms the polarity:
the callee runs "when `ldob(g7 + 0x2a) == 0`".

## Why the refusal is at the own ret slot, not the call slot

Path B follows the cf04 v0747 doctrine and the per-step-loop
contract (`hybrid.c:589-606`): body moves applied, exit at
the function's own `ret`, next `vf2_i960_step` of that `ret`
is the frame-correct pop. Refusing at the `0x61b98` call
slot instead would hand the callee to the interpretation
fallback — not done, per the v0761 retraction (refusal
belongs at a frame-correct IP; the v0763 forced-pop attempt
broke exactly this shape and was reverted). This slice is
strictly cleaner than cf04: there is no post-call tail, so
the ONLY skipped behaviour is the unrecovered call itself.

The refused callee `0x61ba0` was measured, not guessed:
`vf2i960 function` gives 348 B / 12 blocks / `indirect=no`,
`0x61ba0..0x61cfc`, with its own sub-calls to `0x7b18` /
`0x7c60` and display-list stores through `(g11)[g12]`.
Corroboration that `g4`/`g5` are genuine call arguments
(read off that disasm, not game knowledge): `0x61ba0`
tests a bit of `g4` (`bbc r4, g4, 0x61cd8`) and stores
`g5` into the display list (`st g5, (g11)[g12]`).

## Deliberate omissions (documented, not invented)

- **`compare_result` not published** on either path,
  matching the `slot3_table_lookup` v0777 precedent (three
  compare-branches, no CC). No caller evidence requires CC
  across this call (callers: six sites `0x1dec4..0x1edbc`),
  and the legacy/arch stepper CC asymmetry (v0733f/v0733g)
  makes hand-computed CC a fixture that cannot fail.
- **Local `r3` not written back.** Matches cf04/slot3
  precedent; frame-local by construction.
- **Not wired into `g_callee_hooks[]`.** Standalone execute
  hook only, like all v0764–v0782 leaves. The per-step loop
  is dormant on every current test path (v0755c); wiring
  without a reaching snapshot would be untestable surface.

## Post-ret coincidence (pinned, do not "fix")

`ret 0x61b9c + 4 = 0x61ba0` numerically equals the refused
callee's entry. Path A legitimately lands there with the
frame popped; path B never produces that ip (refuses at
`0x61b9c`). A future reader seeing `ip == 0x61ba0` after
`VF2_OK` must not conclude the callee ran.

## Test

ctest #74: `vf2_rob_kage_disp_outside_61b88` PASSES (0.05 s).
4 legs: A1 flag `0x01` admitted; A2 flag `0xff` admitted
(gate is `!= 0`, not `== 1`); B flag `0x00` refused with
`g4 == *(g7+0x684)`, `g5 == 2`; C entry-guard negative
control (wrong ip refuses with zero side effects).

**Gate integrity (planted defect):** `g5 = 2` mutated to
`g5 = 3` in `hybrid.c` → ctest #74 FAILS as required;
reverted → PASSES. The `vf2_python_factory_block_coverage`
gate stays green with the new `functions.csv` row live.

## File map

- `src/recovered/hybrid.c`: `hybrid_execute_rob_kage_
  disp_outside_61b88` + `vf2_hybrid_rob_kage_disp_outside_
  61b88_execute` (+116, pure insertion).
- `include/vf2/hybrid/player.h`: declaration (+11).
- `decomp/i960/functions.csv`: row `0x61b88..0x61ba0`
  (empty trailing `return_to`).
- `tests/recovered/test_rob_kage_disp_outside_61b88_
  native.c`: NEW, 4-leg unit test.
- `CMakeLists.txt`: ctest #74 (+5).

`git diff --stat`: 133 insertions, 0 deletions + 1 new file.
`docs/UNCOVERED_BRANCHES.md` untouched, matching v0765–v0782
leaf practice (last touched at v0733g).

## Next in the gap (not this slice)

Nearest v0783 neighbours by tractability: `reject_sample`
`0x53884` (96 B, 6 blocks — ends in `bx (g14)`, needs the
return-idiom check first), `rob_kage_disp_outside`'s caller
family at `0x1dec4..0x1edbc`, and the small unnamed subs
(`sub_00050130` 44 B, `sub_0005271c` 88 B — naming wants a
second signal per the two-source rule). The `0x61ba0`
callee itself (348 B, display-list writes) is the natural
follow-up that would admit path B end-to-end.
