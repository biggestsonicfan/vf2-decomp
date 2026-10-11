# v0787 - 0x7c60 set_obj recovered, oracle-proved

## Scope

Recovered `0x7c60` (180 B / 5 blocks / ~40 instr) as a 1-to-1
C translation. Shipped Sega symbol (`original_symbols.csv`
record 34); no provisional name existed. All three paths
admitted, no sub-calls. Called from the v0784-refused
`0x61ba0` body (0x61ba4, 0x61cc8) and seven object/display
sites: second cascade step toward admitting v0784 path B
(what still refuses that body afterwards is its own display
stores, not another sub-call).

## Paths

- Early (`*(0x50101c) > *(0x501018)` unsigned): ret with no
  stores, +4 instr, final CC GREATER.
- g1 == 0: body minus block 0x7cd8, +28 instr, CC EQUAL.
- g1 != 0: full body, +32 instr, CC LESS.

## Verified-against-source semantics (then oracle-proved)

- `cmpobg`/`cmpobe` are compare-ordinal-and-branch (the `b`
  is "branch", not "byte"): directions in
  executor_arch.c:61-120.
- `subo s1, s2, d` = s2 − s1 (executor.c:752-753), so
  `subo 1, 0, r11` = 0xFFFFFFFF (re-verified; same finding
  as v0755f).
- `shro` is logical; the `and` mask applies AFTER `r3` is
  derived from the unmasked quad word.
- Entry r9 is DEAD: `ldq` fills r8-r11 before block 0x7cd8
  reads r9 — no caller live-in beyond g0/g1/g10/g11/g12.
- `lda` forms are pure address math. Plain-index scales read
  as shift 0 (`(g11)[g12]`, `(g10)[g12]`, `(g10)`).
- Dedicated `ret`, all callers use `call`: frame popped via
  `vf2_i960_cpu_return_procedure`, no local written back. g0
  clobbered to the table address; g1/g10/g11/g12 read-only.
- CC/AC published every path. cmpob forms use an
  `arch_set_compare` identical to the legacy one, so both
  steppers agree exactly (unlike bbs/bbc).

## Two fixture escapes in one slice (both caught, both fixed)

1. **Unreverted plant looked like an oracle divergence.**
   The mid-slice plant (`r11 = 1-0`) was still live when the
   first oracle run reported `work-ram ... expected=0xff
   actual=0x01`. Byte-level reading resolves it: ref word
   0xFFFFFFFF vs nat word 0x00000001 differ in 4 bytes, and
   the print shows the first differing byte pair. A scratch
   stepper trace (since removed) confirmed the reference
   produces FFFFFFFF at the stq. Lesson: revert before
   diagnosing; read byte-level diffs literally.
2. **Synthetic table poked at the wrong index.** Native body
   legs attached a 1 MB synthetic main-data buffer (v0776
   precedent) but poked the quad at the table base while the
   recovery reads entry 2 (+32). All table-derived native
   checks went red on zeros. Fixed by poking at
   `base + idx*16`. (The matching MSVC rule from v0785 held:
   arithmetic stays outside `UINT32_C`; a `TBL_INDEX` vs
   `TBL_IDX` typo was caught at compile time.)

The plant then did its real job: with body legs running
ROM-free against the synthetic table, the SAME plant fails
BOTH entries (#77 and #106) — the native gate is no longer
blind to body defects. Reverted to green.

## Test

- ctest #77 `vf2_set_obj_7c60` (ROM-free): early/skip/full/
  guard, PASSES (body legs via synthetic table).
- ctest #106 `vf2_set_obj_7c60_differential` (ROM-guarded):
  3 oracle legs FULL MATCH on registers, CC/AC, frames,
  memory and counters, PASSES.
- **Gate integrity:** `r11 = 0-1` planted as `1-0` fails
  BOTH entries; reverted green.

## File map

- `src/recovered/hybrid.c`: `hybrid_execute_set_obj_7c60` +
  `vf2_hybrid_set_obj_7c60_execute` (pure insertion).
- `include/vf2/hybrid/player.h`: declaration.
- `decomp/i960/functions.csv`: row `0x7c60..0x7d14`.
- `tests/recovered/test_set_obj_7c60_live.c`: NEW, dual-mode
  (native always incl. synthetic-table body legs, oracle
  with ROM dir).
- `CMakeLists.txt`: unconditional entry + ROM-guarded
  differential entry.

`docs/UNCOVERED_BRANCHES.md` untouched, matching leaf practice.

## Next (not this slice)

With `set_mmode` (v0786) and `set_obj` (v0787) both
recovered, the `0x61ba0` body's two sub-calls are callable;
admitting v0784 path B end-to-end now wants the `0x61ba0`
body itself (348 B display-list builder, no further
sub-calls beyond these two). Separately, `make_action_table`
`0x520d0` waits on `recog_sample`.
