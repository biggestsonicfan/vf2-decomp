# v0770: study — what remains to deal with after v0769

**This is a pure-research note, no code change.** It
catalogues the remaining tractable advances the next agent
can attempt, with measured evidence and a dependency map.
All slices below were identified during the v0764..v0769
session as tractable but not exercised in that session.

## The committed state

The session closed at `f0e4fbed` (v0769). The active Slice 4
chain added 5 recoveries (v0765..v0769) plus a `task_object`
cluster (v0767) and the `0x1fcc0` wiring analysis (v0764b).
Slices 1, 2, 3, 5, 6, 7 from `advance_plan_v0764.md` remain
deferred for the reasons below.

## Tractable-but-not-attempted candidates

### `sub_00009480` — table-based CRC16 hash, 80 B, 4 callers

**Discovered**: cf04-call brothers at 0x6de3c, 0x6de6c,
0x6de90, 0x6dec0 (per `vf2i960 xrefs`).

**Body shape (per `vf2i960 disasm 0x9480 20`)**:

```
00009480  mov      0, r3                  ; r3 = 0 (running sum)
00009484  lda      0x0000ff00, r10         ; high-byte mask
0000948c  lda      0x000000ff, r11         ; low-byte mask
00009490  subo     1, g2, r9               ; r9 = g2 - 1 (loop counter)

loop (block_00009494):
  00009494  shlo     8, r3, r5             ; r5 = r3 << 8
  00009498  and      r10, r5, r5           ; r5 &= 0xff00
  0000949c  shro     8, r3, r3             ; r3 >>= 8 (drop low byte)
  000094a0  ldob     (g0), r6              ; r6 = next byte (g0)
  000094a4  addo     1, g0, g0             ; g0 += 1
  000094a8  addo     g1, g0, g0            ; g0 += g1 (stride)
  000094ac  xor      r6, r3, r3            ; r3 ^= r6
  000094b0  and      r11, r3, r3           ; r3 &= 0xff
  000094b4  ldos     0x02000000[r3*2], r3  ; r3 = table[r3*2] (16-bit)
  000094bc  xor      r5, r3, r3            ; r3 ^= r5 (combine halves)
  000094c0  cmpdeci  0, r9, r9             ; r9 -= 1
  000094c4  bl       0x00009494

000094c8  mov      r3, g0                 ; return value
000094cc  ret
```

**Tractability assessment**:
- 80 B, 3 blocks, 4 callers — most-called untested leaf.
- Memory model: `0x02000000` is `VF2_MAIN_DATA_BASE` per
  `include/vf2/model2a.h:42` (readable).
- The loop body is a 16-bit XOR-shift accumulator with a
  256-entry table at `0x02000000` (the model-2a MAIN_DATA
  region). The combination has the structure of a CRC16
  but with a non-standard shift pattern (16-bit
  accumulator split between r5 and r3 with `shlo 8` / `shro
  8`).
- **Risk**: 1-to-1 translation has no semantic guidance. The
  test can be set up with a known input buffer and a known
  expected hash, but determining the expected hash requires
  running vf2i960 execute on the same input — i.e., the
  recovery would need vf2i960 as oracle rather than the
  i960 rom-anchor.
- **Mitigation**: write the recovery first (1-to-1
  translation), then run vf2i960 execute(0x9480, n_steps)
  with the same input, comparing step counts and the final
  r3 value. They should match if the recovery is correct.

**Estimated scope**: 1 commit, ~80 LOC hybrid.c + ~80 LOC
test. ~0.5 day.

### `sub_0005ff7c` + `sub_0006d0a0` — Model 2A mirror chain

`sub_0005ff7c` (40 B, 1 block) calls `0x9480` (the leaf
above) once. Recovering it requires `0x9480` to be recovered
first.

`sub_0006d0a0` (108 B, 1 block) at `0x6d0a0..0x6d108` calls
both `0x6d1b4` (recovered v0769) AND `0x5ff7c`. So:

```
0x6d0a0 → 0x6d1b4  (recovered)
0x6d0a0 → 0x5ff7c → 0x9480  (needs 0x9480 first)
```

The cascade is **0x9480 → 0x5ff7c → 0x6d0a0** (also
`0x6cf00`, `0x6cc00`, `0x6c??` siblings).

### `sub_0006cf00` (524 B) and `sub_0006cc00` (620 B)

Both are Model 2A memory cluster writers in the v0734k
gap run `0x6cb0c..0x6dcb8`. Both call `0x5ff7c` + have
internal logic. Recovering either unblocks multiple gap-run
recoveries.

### `sub_00009444` (inline_text_thunk) — 11 callers

Smallest/most-called untested. But **`indirect=yes`** at
the trailing `bx (r14)` instruction (indirect branch). The
`vx_v2_do_text_inline` style of this function is
documented as an "inline text" helper. Recovering this
requires either:

- Refusing the indirect branch path in the test (clean
  refusal at the `bx (r14)` instruction), OR
- Inlining the loop body so the ret at the function end
  is achievable without the `bx`.

**Risk**: the indirect branch is load-bearing for the live
callers; refusing it would diverge from the live path.

### Cf04-clearing siblings beyond v18/v19

A directed search of `vf2i960 disasm` in `0x32000..0x34000`
found only 2 cf04-call + clrbit-N-r13-r13 patterns:
0x32284 (v18, recovered v0756b) and 0x323fc (v19,
recovered v0756).

3 MORE cf04-call sites exist in `0x33000..0x33400` at
0x33134, 0x332b0, and 0x33464 — but they use **r15**, not
r13, in their post-call work. They are a different
family, **not direct v17/v20/v21/v22 siblings** of
0x32284/0x323fc.

The hitbox/hurtbox (`+0x61e`/`+0x626`) per
`phase_4_simulation_boundary_v0738.md` lives in the
state-25 walker (`fa_player_1442c`). The walker is
recovered; the hitbox body inside it is a small
self-contained function recoverable from the disasm, but
the offsets `+0x61e` / `+0x626` are fighter-record offsets
in work-RAM, not instruction addresses — finding the
hitbox function requires tracing inside the walker from
the state-25 dispatch.

## Slice 1 / 2 (0x1fcc0 wiring) — DEFERRED

The v0764b wiring analysis pinned Precondition 1 (verified
this session: `0x1fee0` IS `ret 0x0a000000`) and Precondition
3 (both call sites `0x99fc` and `0xcfa0` use `call 0x1fcc0`,
not `b`). Precondition 2 (frame-push from the recovery's
`enter_procedure(0x1fee4, 0x1ff10)` interferes with the
entry's `ret` at `0x1fee0`) remains unresolved.

Per `advance_plan_v0764.md` these slices are last because of
the two prior failed attempts (v0761 hook wiring, v0763
UNSUPPORTED hardening). A third attempt must plant a
defect and verify a ctest FAILS on the planted defect
before shipping the patch.

## Slice 5 — Phase 4 first chunk (hitbox/hurtbox)

Pending the state-25 hitbox function trace (see above).
Once a hitbox function is identified via disasm, the
recovery is tractable: small (~100 LOC), recoverable
1-to-1 from disasm, with a ctest that exercises the
state-25 dispatch from `fa_player_1442c` and verifies the
hitbox writes at `+0x61e`/`+0x626`.

## Slice 6 — 87 KB gap characterisation at `0x4ec00..0x640f4`

Already extensively documented in
`decomp/i960/notes/gap_characterisation_v0734h.md`
(probes at 0x4ec00, 0x56800, 0x1200..0x1290, 0x12bc..0x12d8).
A v0764 characterisation note would duplicate, not add.

## Slice 7 — corridor extension past `0x164c4`

`vf2i960 function 0x164c4` is the v0730 next entry but it's
a 64-B fall-through tail of `task_game_info`. The next
REAL entry candidate is `sub_00016504` at `0x16504..0x176a0`
(4,668 B, 126 blocks) — too large for autonomous work. The
corridor extension requires multi-iteration taint-driven
discovery on the recovered corridor's traces, plus recovery
of one or more `sub_00016504` sub-leaves.

## Recovery tooling status

The factory layer (`tools/python/frontier.py`,
`infer_structs.py`, `taint.py`) was confirmed working in
v0729 with 18 unit tests passing. It identifies
**next-target candidates** but doesn't itself produce
recoveries — it ranks them. To re-trigger frontier-driven
recovery, run `python tools/python/frontier.py --limit 20
--cross-boundary --exclude-recovered
out/trace-f0.jsonl out/trace-both.jsonl` after a fresh
trace is generated on the recovered corridor.

## What this study concludes

The five commits landed in this session (v0765..v0769)
were tractable because each was a small, well-bounded
parseable function. The remaining tractable candidates
are **cascading** (each depends on a previous one) or
**structured-boundary-coupled** (Slice 1/2 with the
frame-push conflict, Slice 5 with the state-25 hitbox
trace). A next-session strategy is:

1. Recover `sub_00009480` first (the CRC leaf, simplest
   self-contained). Validates against vf2i960 execute
   as oracle; a passing test confirms the translation is
   exact.
2. Recover `sub_0005ff7c` next (uses 0x9480).
3. Recover the gap-run siblings `sub_0006cf00` and
   `sub_0006cc00` (use 0x5ff7c).
4. With the cascade complete, attempt `sub_0006d0a0`
   (uses both 0x6d1b4 [done] AND 0x5ff7c).
5. Only THEN consider Slice 5 (Phase 4) and Slice 7
   (corridor extension), which require larger
   evidence gathering.
6. Slice 1+2 (0x1fcc0 wiring) remains last because it
   is a repository-state-sensitive change that has
   already failed twice.

This sequencing keeps each step's risk proportional to
its evidence base.
