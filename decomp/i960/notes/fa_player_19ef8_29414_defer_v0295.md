# v0295: measure 0x19ef8 flag-bit siblings and 0x29414 non-zero, defer

## Verdict

Both named targets from `fa_player_next_targets_v0292.md` are **deferred**
for measurement reasons. No C recovery and no guard relaxation.

## `0x19ef8` flag-bit siblings

### What was tried

Drive from `out/player-14288-rt.vf2snap` (confirmed parked at `0x14288`,
fighters `0x510800`/`0x510980`) with `--set-u32` on:

- `+0x1a4` bits 5 / 6 / 21 / 23 and combinations
- `+0x000` (player_flags) bits 5 / 6 / 21 / 23

### Result

Every case — **including the unmutated baseline** — takes the same
interpretive path through `0x19ef8` → `0x1a1e4` and then **memory-faults
at `0x0002704c`** (`stos r5, (r4)`) after exactly **980** instructions.

The parked snapshot does not support a full interpretive replay of the
`0x19ef8` corridor. The accepted C path is proven by the whole-task PUNCH
differential (hybrid native vs live reference), not by isolated replay.

### Implication

Sibling shapes cannot be measured from this park. The C guards on
`player_flags` bits 5/6/21/23 and `+0x1a4 != 0` stay fail-closed.
Relaxing them needs either:

- a live hybrid-context drive (mutate, run `vf2cycles`, read per-block
  counts when C returns `UNSUPPORTED` and falls back), or
- a park whose reference can complete `0x19ef8` to `0x1428c`.

## `0x29414` non-zero path (types 6 / 8 / 10)

### What was tried

1. `out/player-29414-{sixth,punch}.vf2snap` are **not** at `0x29414`:
   both sit in the main wait loop `0x10F98`/`0x10FA0`
   (`ldob 0x500000` / `cmpibe`).
2. Re-park from `player-14288-rt` → `0x29414`: faults at `0x2704c`
   after 980 insns (same blocker as above).
3. Re-park from `punch10` with `--max-steps 5000000`: still ends in the
   `0x10FA0` wait loop. The reference has no IRQ injection in `vf2probe`,
   so it never exits the wait-for-vblank spin that `vf2cycles` breaks via
   its frame-wait IRQ path (`Frame-wait ... IRQs: 350` on PUNCH).

### ROM shape (already disassembled, still valid)

```
29414  ldob +0x1b1(g7), r15
29418  cmpobe 10 → 29478
2941c  cmpobe  6 → 29478
29420  cmpobe  8 → 29430
29424  mov 0, r14 / st +0xc50 / b 29548   # zero path (native)
29430/29478  constants into r10,r9,r8,r13
29498  ld +0x1a4(g7), r14
2949c  bbc 19 → 29538   # +0xc50 = (+0x84)*(r9-1)
294a0  ldos +0x1aa(g7), r12
294a4  cmpobl 20 → 294f8
294a8  cmpobge 10 → 29538
294f8  ld 0x508000 / bbs 5 → ret
29538  mulr/subr/st +0xc50 / ret
```

`0x294ac..0x294f4` remains unmeasured from this entry — do not copy.

### Implication

`hybrid_execute_player_29414_zero_path` keeps returning
`VF2_ERROR_UNSUPPORTED` for types 6/8/10. No native non-zero path.

## Pins

Unchanged and re-validated after the sibling/29414 probes and after the
v0294 `0x1a1e4` integration:

- PUNCH **320/320 MATCH**, 12946 blocks, **14.962.620** insns, both `0x1645c`
- ctest Debug **56/56**

## Next measurement path

1. Hybrid-context sibling drive: copy `punch10`, set `+0x1a4`/`+0` bits at
   a scheduler boundary, run `vf2cycles` a few cycles, read the hybrid
   report for fallback instruction counts.
2. For `0x29414`, same pattern with `+0x1b1 ∈ {6,8,10}` set before the
   player task; the zero-path C will return `UNSUPPORTED` and the hybrid
   will interpret the small non-zero body in the live machine context.
3. Only after a compact shape is measured, write C and prove it with a
   focused differential fixture.
