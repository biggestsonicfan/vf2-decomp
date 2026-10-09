# v0764: per-step hook wiring analysis — what v0761, v0763, and the current dormant state teach

**Status: research note, no code change.**

This documents the principled understanding of the per-step
hook wiring state, so a future autonomous attempt at Slices
1+2 of `advance_plan_v0764.md` starts from a settled
analysis instead of repeating the broken v0761 / v0763 cycle.

## The current state

`hybrid_range_has_hooks` at `hybrid.c:518` is a **no-wrap**
range check:

```c
if (h >= entry_address && h < stop_address) {
    return 1;
}
```

For the post-`0x29414` corridor `hybrid_execute_interpreted_until
(0x28178, 0x14400)`, the entry address is greater than the stop
address, so the range is empty (no hook entry_ip satisfies both
parts). The function returns 0, and `vf2_i960_run` is used
instead of the per-step loop. **All four wired hooks
(`0x29598`, `0x439ac`, `0x43888`, `0xcf04`) are dormant in
this corridor.**

The F4 INDIVIDUAL release test passes — but only because the
legacy `vf2_i960_run` path executes the corridor, not because
the per-step hook fires.

## What v0761 actually did

`v0761` added a 5th entry to `g_callee_hooks[]` for
`0x1fcc0`. With no-wrap `hybrid_range_has_hooks`, adding the
entry has no effect on `hybrid_execute_interpreted_until
(0x28178, 0x14400)` — the hook table still returns 0 and
`vf2_i960_run` is used.

But the v0761 retraction note documents that v0761's F4 test
hit a >600 s hang. This is the documented observation; the
retraction commit was `git reset --hard` to a state that
predates v0761, so the actual diff is no longer in history.
The retraction note's wrap-around analysis is a **post-hoc
reconstruction**, not a measured diff.

**What I can verify from the current code**: the F4 test
passes today with hooks dormant. The only way v0761 could
have changed behaviour is if v0761 also modified
`hybrid_range_has_hooks` to a wrap-around check. The
retraction note's `hybrid_range_has_hooks` snippet uses the
wrap-around form, so this is consistent with v0761 having
added both the hook entry AND the wrap-around check, then
both being reverted together.

## What v0763 did wrong

`v0763` (fef46004) modified only the per-step loop's
UNSUPPORTED branch to call `vf2_i960_cpu_return_procedure`
(forcibly pop the call's frame). This was option 2 of the
v0761 retraction note. The diff was reverted in aa928e44.

**Why it broke cf04**: cf04 (v0747) refuses at `0xcfb8`,
which IS the entry's own ret slot. The per-step loop's
next `vf2_i960_step(cpu, ...)` of that `ret` correctly
pops the call's frame (because cf04 was actually called).
v0763 popped the frame BEFORE the ret could run, leaving
the caller's frame on top for the ret to pop instead.

**The general lesson**: a refused recovery must land at
`cpu->ip = entry_ip + 4` (the byte after the `call`) ONLY
if the loop can detect this. Otherwise, the recovery's
`cpu->ip` placement determines correctness.

## The principled fix (Option 1 of the v0761 retraction note)

`v0761` retraction option 1: make `0x1fcc0` recover to
`VF2_OK` by fully simulating the `0x2c38` sub-call AND the
three remaining sub-calls (`0x4b410`, `0x2eab8`, `0x11704`)
that come after the current refusal point. Set `cpu->ip`
to the entry's own ret slot (`0x1fee0`) and return
`VF2_OK`.

This is now feasible because v0755f recovered `0x2c38`
(`color_table_rebuild`) — the only sub-call the original
v0755g refused.

**Two preconditions** (must be confirmed before the patch):

1. **`0x1fee0` is a `ret` instruction**. Per the v0755g
   note, the function's extent is `0x1fcc0..0x1fee4` (548 B,
   15 blocks). The 4 bytes from `0x1fee0..0x1fee4` is the
   natural position for a single `ret` instruction
   (i960 `ret` encoding is 4 bytes). This MUST be
   confirmed by `vf2i960 disasm 0x1fee0` before the patch,
   not assumed.

2. **The recovery's pre-refusal work must mirror the
   i960 hardware on entry to `0x1fcc0`**. The current
   v0755g recovery's `cpu->ip` at refusal is `0x20050`,
   the ret slot of the inlined `0x1fffc`'s refusal of its
   sub-call to `0x2c38`. After the patch, the recovery
   must set `cpu->ip = 0x1fee0` AND ensure the per-step
   loop's next step of that `ret` correctly pops the
   `call 0x1fcc0` frame.

   For `call 0x1fcc0` (the i960 hardware's behavior): the
   `call` pushes a frame containing g14 (return-to) =
   instruction-after-`call`. The `ret` pops that frame and
   sets `cpu->ip = g14`.

   The recovery's pre-`ret` work MUST NOT push
   additional frames that would also be popped by the ret.
   v0755g already does push one frame (for 0x1fee4 inline
   via `enter_procedure` at line 34766). That frame is
   the one that gets popped by the entry's ret — but
   the i960 hardware's `call 0x1fcc0` would have pushed
   a DIFFERENT frame, the one belonging to 0x1fcc0 itself.
   
   **There is a frame inconsistency**: the recovery
   pushes a frame for `0x1fee4` (an inlined sub-call)
   but the i960 hardware wouldn't have pushed such a
   frame (0x1fee4 is inlined, not a separate `call`).
   The ret at `0x1fee0` (entry's own ret) would pop the
   recovery's pushed frame, NOT the i960-hardware's
   `call 0x1fcc0` frame.

   For this to be sound, the `call 0x1fcc0` frame
   must be the SOLE thing on the stack at the moment
   the entry's ret runs. The recovery's currently
   pushed frame for `0x1fee4` is NOT additional
   stack-pushing done by the i960 hardware — so it must
   be popped BEFORE the entry's ret. The current
   recovery does NOT pop it (line 34822 explicitly
   skips `return_procedure`).

3. **`0x1fcc0` is entered via `call`, not via
   `b 0x1fcc0`**. If the i960 hardware branches to
   `0x1fcc0` (no frame), there's no `call 0x1fcc0` frame
   to pop, and the ret at `0x1fee0` would pop the wrong
   frame (or underflow the stack). The disasm at the
   actual call sites must be inspected to confirm
   `call 0x1fcc0` is the entry mechanism.

These three preconditions are non-trivial verification
work, NOT just a patch. A future autonomous attempt at
Slice 1 of `advance_plan_v0764.md` must verify all three
before changing `hybrid.c`.

## Why range-check fix is independent but related

Making `hybrid_range_has_hooks` wrap-around would make
the per-step loop FIRE for the (0x28178, 0x14400)
corridor. **This is a separate slice** (call it Slice 1'
in advance_plan_v0764). It would expose the dormant
hooks to the live path. **Without confirming that the
existing 4 hooks behave correctly under vf2_i960_step
(the arch stepper per v0755b), this change risks
introducing cc-state divergences on the F4 path.**

The v0733f / v0733g analysis established that
`vf2_i960_run` (legacy stepper) and `vf2_i960_step`
(arch stepper) diverge on `compare_result` for some
inputs. v0733g's measurement showed that running the
suite with `VF2_LEGACY_STEP_IN_RUN=OFF` makes 19 of 119
tests fail on `compare_result` only. The recovered
`hybrid.c` was written against the legacy stepper.

**Therefore: a range-check fix would expose the existing
4 wired hooks to the diverging stepper, and would
break tests.**

This is why the v0764 plan defers Slices 1+2 to LAST.
Before they can be attempted, the per-step loop's
cc-state divergence with `vf2_i960_run` must be
understood and accommodated.

## Validated

- `git show hybrid.c:518` confirms no-wrap check.
- `git log aa928e44^..fef46004` confirms v0763 modified
  only the UNSUPPORTED branch, then reverted.
- `v0755g` note line 34931 confirms `cpu->ip = 0x20050`
  is the current refusal point.
- `v0755g` note line 34766 confirms the recovery does
  `enter_procedure(0x1fee4, 0x1ff10)` for the inlined
  `0x1fee4` sub-call.
- All cited evidence is in `git log`, the existing
  notes, and the current `hybrid.c` text. No synthesis.

## What this slice does NOT claim

- **No code change.** This is purely a research /
  analysis note.
- **No advance on the actual recovery.** It is a
  prerequisite analysis for Slices 1+2 of
  `advance_plan_v0764.md`.
