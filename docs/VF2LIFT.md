# vf2lift — build-time i960→C recompiler (Phase 4, Option B)

Status: in progress (2026-10-14). Gate **G1** was decided by the maintainer
(celsowm) in favour of **Option B**: a build-time recompiler. This document is
the design and the running record of what is built. It is the companion to
`docs/NATIVE_WINDOWS_PLAN.md` Phase 4.

## What it is, and what it is not

`vf2lift` reads the user's own ROM at build time, walks the control-flow graph,
and emits one C function per reachable guest function into the **build
directory only**. Nothing it generates is ever committed — exactly the position
of the interpreter, which also reads the ROM at run time. The generated C is a
*bridge* that lets the game run with native, compiled control flow while hand
recovery continues; the readable hand-recovered C stays the canonical product
and overrides any lifted function through the dispatch registry.

It is **not** a second implementation of the i960 instruction set. Each lifted
basic block replays its instructions through `vf2_i960_step_lifted`, which is
`vf2_i960_step` with one change — the retired instruction is counted as *lifted*
rather than *interpreted*. The opcode semantics, the architectural fix-ups
(`cmpob`/`cmpib`/`bbs`/`bbc`, `bo`/`bno` after a compare), and the call/return
frame mechanics therefore come from exactly one place, `src/i960/executor.c`
plus `src/i960/executor_arch.c`. This is deliberate and follows the plan's own
wording ("using the same instruction semantics as `src/i960/executor.c`") and
its clean-room risk note ("option B must not erode the clean-room goal"): a
hand-written C re-expression of every opcode would be a second source of truth
that could silently diverge from the oracle. There is none.

What the lift actually contributes, then, is **structure, not semantics**:

- one C entry point per guest function, so a call is a compiled C call through
  the dispatch registry rather than an interpreter re-entry;
- native compiled control flow between basic blocks (a `switch` on `cpu->ip`
  over the small set of CFG successors), instead of the interpreter's generic
  decode-and-follow loop;
- calls and returns routed through the existing architectural frame model
  (`procedure_call` / `procedure_return`, via the shared step);
- indirect transfers resolved from the recovered jump tables, or routed through
  a checked dispatcher that fails closed on anything unmodeled.

Because lifted code is interpreter-backed, it is **scaffolding** in the same
sense the interpreter is: it may not appear in a shipped build. It is opt-in
behind the CMake option `VF2_ENABLE_LIFTED` (default **OFF**) and is tracked
alongside the interpreter in the "no HLE / no emulation at ship" accounting. The
thing that retires scaffolding is hand recovery: a recovered function overrides
its lifted counterpart.

## The instruction-accounting model

The i960 CPU carries three retirement tallies (`include/vf2/i960/executor.h`):

- `executed_instructions` — every guest instruction retired, by any engine
  (serialized in the snapshot; it carries the boot baseline).
- `interpreted_instructions` — the subset retired by the interpreter loop
  (`vf2_i960_step` / `_legacy` / the arch literal-move path). Host-side only.
- `lifted_instructions` — the subset retired by lifted C through
  `vf2_i960_step_lifted`. Host-side only.

so, for the shipped hybrid runtime,

```
recovered = executed_instructions - interpreted_instructions - lifted_instructions
```

Hand-recovered blocks advance `executed_instructions` by their modeled amount
and touch neither of the other two, so they fall into `recovered` by
construction. `interpreted` and `lifted` both measure scaffolding still in the
loop; Phase 4 (Option B) progress is the two of them trending to zero as
recovery proceeds, and the exit condition is **`interpreted == 0`** for a
session, with `lifted` then being the only bridge left and itself retired in
coverage order during Phase 5.

`vf2_i960_step_lifted` delegates to `vf2_i960_step` and then moves the single
retirement it caused from the interpreted tally to the lifted tally; a fault
retires nothing and leaves both unchanged. It is proven differential-neutral:
the whole non-fixture test suite (133/133) is unchanged by its introduction.

## Pipeline

```
user ROM ─▶ vf2lift (build time)
             │  1. load MAINCPU image (addr == offset, ROM base 0)
             │  2. CFG + boundaries + jump tables  (reuse src/analysis:
             │       vf2_i960_analyze, vf2_detect_function_boundaries,
             │       vf2_i960_run_semantic_analysis / indirect_targets)
             │  3. per guest function: emit a C function whose body is the
             │       block CFG, each block a run of vf2_i960_step_lifted calls,
             │       inter-block edges compiled as a switch on cpu->ip, calls
             │       dispatched through the registry, indirect via jump tables
             │       or the fail-closed dispatcher
             │  4. emit a registration table { entry address -> C function }
             ▼
   <build>/generated/vf2_lifted.c (+ .h)   ← never committed
             │  compiled into the build when VF2_ENABLE_LIFTED=ON
             ▼
   dispatch registry:  recovered  >  lifted  >  interpreted
```

The input set of entry points is the Phase 3 coverage union plus the recovered
registry, so vf2lift only ever emits functions the measured corpus actually
reaches. A function is admitted to the lifted registry only if its per-function
differential passes (below); one that does not match stays unlifted and the
runtime falls back (lifted → interpreted) for it.

## Per-function differential (the admission gate)

For each candidate function, from a state captured at its entry in a Phase 3
session (`vf2_i960_snapshot_capture` / `restore`), run the lifted function and
the pure interpreter from the identical state and compare the full CPU and every
memory write (`vf2_i960_snapshot_compare` / `vf2_i960_compare_live_state`, and
the existing `native_differential` harness). Registers, condition state, the
frame stack and work RAM must match. Because the lifted body executes the same
semantics as the interpreter, a straight-line function matches by construction;
the differential is a real check on the *structure* — block boundaries,
successor selection, call/return routing and jump-table resolution — and on the
fail-closed handling of anything unmodeled.

## Dispatch integration

The native runtime dispatch (`vf2_native_runtime_step_impl`, an `cpu->ip`-keyed
chain that currently fails closed on unknown IPs) gains a lifted tier below
recovered: when `VF2_ENABLE_LIFTED=ON` and no recovered block claims `cpu->ip`,
a lifted function registered for that entry runs; otherwise the behaviour is
unchanged (fail closed, or interpreter in the validation path). The session
status line reports the three-way split so the bridge is always visible.

## Honest scope

The full Phase 4 (Option B) exit has two halves. The reachable half now:

- lift every function in the Phase 3 **union**, each gated by its differential;
- the attract session runs with `interpreted == 0` (everything lifted or
  recovered), its per-frame work-RAM digests equal to the oracle's.

The other half — "the lifted build plays a session from reset to the end of a
match" — needs the game to advance past the black-frame attract, which needs the
geometry pipeline (**G2 / Phase 6**) and video (Phase 7). That half is gated on
G2 exactly as the Phase 3 remainder (coin/select/match sessions) is, and is not
reachable from this corridor alone. vf2lift is built and validated on the
reachable corridor now; the match-length validation is wired up once G2 lets the
session advance.

## Slices

1. **Foundation (done, 2026-10-14):** `lifted_instructions` counter and the
   `vf2_i960_step_lifted` primitive (differential-neutral, 133/133); this
   document.
2. `tools/vf2lift`: CFG-driven codegen for one function, `VF2_ENABLE_LIFTED`
   wiring, generated C compiled into the build dir.
3. Per-function differential harness and the admission gate over the Phase 3
   union.
4. Dispatch registry (recovered > lifted > interpreted) and the attract-session
   `interpreted == 0` demonstration with the three-way status split.
