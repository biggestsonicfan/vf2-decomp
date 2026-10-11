# v0785 - 0x53884 reject_sample recovered, oracle-proved

## Scope

Recovered `0x53884` (96 B / 6 blocks / 18 instr) as a 1-to-1
C translation. **Second recovery from the v0783 87 KB gap
slice**, under its shipped Sega symbol
(`original_symbols.csv` record 284; no provisional name
existed). All four paths admitted — no sub-calls, no
refusals. Sole caller: `bal 0x53884` at `0x5219c` inside
`make_action_table` (the xref edge prints as "call"; the
disasm is `bal`).

## Why this slice is shaped the way it is

**`bal`, not `call`.** The caller links with `bal`
(executor.c:1211-1215: sets g14, pushes no frame) and the
function returns with `bx (g14)` (executor.c:1219-1238: a
plain jump to g14, pops no frame). So the function runs IN
THE CALLER'S FRAME: local r12-r15 writes persist (the
caller re-loads r12 at 0x521a0, corroborating the clobber),
procedure counters do not move, and the exit ip is g14's
entry value. The recovery writes locals back and returns
g14, pushing/popping no frame — deliberately NOT the
enter/return_procedure convention used for `call`/`ret`
callees. Getting this wrong (a frame pop with depth 0)
would fail the oracle on `local_frame_depth` alone.

**`concmpo`** (executor.c:1071-1093): preserves a preceding
EQUAL (AC bit 1), else compares and replaces. Each
cmpo/concmpo/be chain therefore branches iff the sample
equals EITHER table byte. Every comparison on every path
is an equality, so operand order is immaterial (no v0732
trap possible here).

**Indexed scale** (decoder.c:558, executor.c:201-203): the
disassembler's `[g1]` vs `[r13*2]` forms print the decoded
scale — sample at `g13+0x5c+g1`, table at `0x50912+r13*2`.
Verified in the decode dump during diagnosis
(`sc=0 idx=17 base=29` vs `sc=1 idx=13`), then proved by
the oracle legs (a wrong scale lands the 0xff where the
memory diff goes red).

**Final CC/AC published on every path** via a
`hybrid_publish_compare_result` helper that replicates
`set_compare_result` (executor.c:220-236) exactly,
preserving AC bits above bit 2. Paths 1-3 end EQUAL; the
full miss publishes the unsigned compare of the last table
byte against the sample. The body uses no bbs/bbc/cmpob
form, so the legacy/arch stepper asymmetry (v0733f/v0733g,
which rewrites only those forms) cannot diverge here.

## The fixture bug the oracle caught (read this first)

The first version of this slice primed and read
**`0x00500910`** (work RAM) for the disassembly's
**`0x00050910`** (main-ROM constant table, below 2 MB) —
one dropped zero routes the access to a different 4 MB
page. Recovery and fixture were wrong *identically*, so
all 8 native legs passed self-consistently while the
reference read the real ROM bytes. The oracle legs went
red on 6 of 7 paths (L1 sentinel needs no table read and
FULL MATCHed throughout):
`component=cpu-state expected=0x1 actual=0x2` (ref LESS vs
native EQUAL) plus count mismatches on L2-L5, and
`component=registers first_offset=0xe expected=0x4c` on
L6/L6G. Diagnosis (scratch stepper probe, since removed):
ROM bytes at those offsets are exactly
`0x07/0x0c/0x4c/0x4f` — the reference was right.

This is the v0732p trap live-fired in miniature ("a
register pinned to a synthetic address is a fixture that
cannot fail"), except the address was *misread*, not
pinned: the fix was measured, not argued. Restructured
after the fix, per the read-only reality:
- the recovery reads `0x0005091x` (fixed);
- only L1 (sentinel) and L7 (guard) are ROM-free, since
  every other path loads ROM constants;
- oracle legs derive R0/R1/T0/T1 from the attached ROM at
  runtime and premise-guard every leg shape (degenerate
  table fails loudly, coli-live `check_fixture_premises`
  discipline). Measured premises: `07 0c 4c 4f` at idx 3.

## Test

- ctest #75 `vf2_reject_sample_53884` (ROM-free): L1 + L7,
  PASSES.
- ctest #102 `vf2_reject_sample_53884_differential`
  (ROM-guarded `"${VF2_ROM_DIR}"` entry): 7 oracle legs,
  each FULL MATCH on registers incl. locals, CC/AC,
  frames, memory and counters with identical instruction
  counts (4/11/17/16/16), PASSES.
- **Gate integrity:** sentinel `== 8` planted as `== 9`
  fails BOTH entries; reverted to green.

## File map

- `src/recovered/hybrid.c`:
  `hybrid_publish_compare_result` helper +
  `hybrid_execute_reject_sample_53884` +
  `vf2_hybrid_reject_sample_53884_execute` (pure insertion).
- `include/vf2/hybrid/player.h`: declaration.
- `decomp/i960/functions.csv`: row `0x53884..0x538e4`
  (one past `bx (g14)` at `0x538e0`).
- `tests/recovered/test_reject_sample_53884_live.c`: NEW,
  dual-mode (native always, oracle with ROM dir).
- `CMakeLists.txt`: unconditional entry + ROM-guarded
  differential entry.

`docs/UNCOVERED_BRANCHES.md` untouched, matching v0765-v0784
leaf practice (last touched at v0733g).

## Next in the gap (not this slice)

- `0x61ba0` (348 B display-list builder, refused by v0784
  path B) starts with `call set_mmode (0x7b18)` — and
  `set_mmode` itself is a 16 B shipped-name leaf (`mov 0,r3;
  st r3,0x70(g10); st g0,(g10)[g12]; ret`) that would be the
  natural next cascade step before the `0x61ba0` body.
- `make_action_table` `0x520d0` (680 B), the caller that
  `bal`s both `recog_sample` and `reject_sample`, is the
  next rung up once its other callee is characterised.
