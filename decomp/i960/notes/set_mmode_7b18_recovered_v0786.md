# v0786 - 0x7b18 set_mmode recovered, oracle-proved

## Scope

Recovered `0x7b18` (16 B / 1 block / 4 instr) as a 1-to-1
C translation. Shipped Sega symbol (`original_symbols.csv`
record 33); no provisional name existed. Fully admitted, no
sub-calls, no branches. Called from the v0784-refused
`0x61ba0` body (0x61ba4, 0x61cf4) and from 0x117fc/0x11804,
so this is the first cascade step toward admitting v0784
path B end-to-end.

## Disassembly

```
00007b18  mov      0, r3
00007b1c  st       r3, 0x00000070(g10)
00007b20  st       g0, (g10)[g12]
00007b24  ret
```

Extent `0x7b18..0x7b28` confirmed by two sources:
`vf2i960 function` measures `end=0x7b28`, disasm shows
`ret` at `0x7b24` (= end − 4).

## Calling convention (checked after the v0785 lesson)

All four callers use the dedicated `call` instruction and
the callee ends in the dedicated `ret` — the opposite of
reject_sample's bal/bx shape, verified per site rather
than carried over. So the call/ret frame discipline
applies: the test pushes a frame per leg with
`vf2_i960_cpu_enter_procedure`, the recovery pops it with
`vf2_i960_cpu_return_procedure` (the same pop the
reference's `ret` performs), and the `mov 0, r3` evaporates
with the frame. Procedure deltas per leg: +0 calls / +1
return on each side.

## What the oracle arbitrated

- **Plain-index scale**: `(g10)[g12]` read as g10+g12
  (scale 0, the same form v0785 proved for `[g1]`). A
  wrong scale puts g0 where the memory FULL MATCH goes
  red; 3/3 legs FULL MATCH.
- **Instruction total 4** (3 body + ret): asserted against
  the reference count, not just self-consistently.
- **CC/AC passthrough**: no instruction writes condition
  state, so the prime's NONE/`0x3f001000` must survive on
  both sides — asserted in native and oracle legs.
- **Scrub discipline**: both target words pre-filled with
  `0xAAAAAAAA` per leg, so a missing store cannot hide
  behind a stale zero (the complementary trap to v0785's
  synthetic-address fixture: there the values were right
  but the address wrong; here the addresses are trivially
  right, so the risk inverts to unwritten stores).

Legs: L1 (idx 5, mid-range value), L2 (idx 0 + `0xffffffff`
edges), L3 (far idx 0x100, value 1), L7 entry-guard
control (wrong ip refuses, scrub intact, +0).

## Test

- ctest #76 `vf2_set_mmode_7b18` (ROM-free): 3 native legs
  + guard, PASSES.
- ctest #104 `vf2_set_mmode_7b18_differential`
  (ROM-guarded): 3 oracle legs FULL MATCH on registers,
  CC/AC, frames, memory and counters, PASSES first try.
- **Gate integrity:** store offset `0x70` planted as `0x74`
  fails BOTH entries; reverted to green.

## File map

- `src/recovered/hybrid.c`: `hybrid_execute_set_mmode_7b18`
  + `vf2_hybrid_set_mmode_7b18_execute` (pure insertion).
- `include/vf2/hybrid/player.h`: declaration.
- `decomp/i960/functions.csv`: row `0x7b18..0x7b28`.
- `tests/recovered/test_set_mmode_7b18_live.c`: NEW,
  dual-mode (native always, oracle with ROM dir).
- `CMakeLists.txt`: unconditional entry + ROM-guarded
  differential entry.

`docs/UNCOVERED_BRANCHES.md` untouched, matching v0765-v0785
leaf practice.

## Next (not this slice)

With `set_mmode` recovered, the `0x61ba0` body's entry call
(0x61ba4) is callable; what still refuses it is the mid-body
`call 0x7c60` (0x61cc8) plus the unrecovered display-list
stores. `0x7c60` characterisation is the next cascade step.
Separately, `make_action_table` `0x520d0` waits on
`recog_sample`.
