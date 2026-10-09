# v0764: advance plan — seven slices after the v0763 retraction

Written after v0763 was reverted (master at `aa928e44`).
The v0761 + v0763 wiring attempts left the per-step hook
infrastructure dormant: `hybrid_range_has_hooks` at
`hybrid.c:518` is a no-wrap range check, so for the
post-`0x29414` corridor `hybrid_execute_interpreted_until
(0x28178, 0x14400)`, the wrap-around range produces an empty
set and `vf2_i960_run` is used instead of the per-step loop.
Both retractions analysed the wrong assumption. The hooks are
correctly wired, but they are not in any of the ranges the
F4 native path traverses.

This plan is **sequenced by tractability, not by impact**.
Slices 1+2 (0x1fcc0 wiring) are deliberately last because
they are the slices whose two prior attempts at autonomous
recovery shipped broken. The slices in 3-7 advance the
recovery surface through tractable, evidence-bounded work
that does not re-activate dormant infrastructure.

## Slice 1 — Recover `0x1fcc0` to return `VF2_OK` (Option 1 of v0761 retraction)

**Why deferred**: requires inlining the recovered
`0x2c38` (v0755f) AND the recovered `0x4b410` / `0x2eab8` /
`0x11704` bodies after the current refusal point, then
setting `cpu->ip = 0x1fee0` (entry's own ret slot) and
returning `VF2_OK`. The disasm at `0x1fee0` must be confirmed
as `ret` before the patch. ~150 LOC change to `hybrid.c`
+ unit-test expectation rewrite. Two prior autonomous
attempts on related slices (`v0761` and `v0763`) shipped
broken behaviour; this slice must include a fresh failure
plant and a ctest that **fails on the planted defect**.

## Slice 2 — Wire `0x1fcc0` into `g_callee_hooks[]`

**Depends on Slice 1**. Add the 7 entries (0x1fcc0 +
0x1fee4 + 0x1ff0c + 0x1fffc + 0x4b410 + 0x11704 + 0x2eab8 +
0x2c38) and verify the per-step loop fires them on the F4
corridor. Will require the range-check fix (no-wrap must
become wrap-around) which is itself an invasive change.

## Slice 3 — `0x29414` post-cf04 sibling cascade

`0x323fc` (v0756) and `0x32284` (v0756b) are the recovered
siblings for clrbit 19 and clrbit 18. `taint.py` on the
post-0x29414 trace likely flags siblings for clrbit 17, 20,
21 (and others) in the `0x32xxx` neighbour. Each new
sibling is ~50 LOC of C plus a unit test. Factory layer
(`frontier.py v2`) produces the per-source attribution so
the recovery has a proven provenance.

## Slice 4 — Recover v0734k's characterised gap runs

`phase_1_3b_gap_characterisation_v0734k.md` characterised
4 real functions as 5,448 / 5,456 / 4,524 / 29,120 B. Each
is reachable through `vf2i960 disasm` and bounded. Each
adds a `functions.csv` row (with the v0734a two-source rule
applied — `vf2i960 function` extent AND `vf2i960 disasm`
showing `ret` at `extent - 4`) and a ctest entry. **This
slice is the highest-leverage coverage delta per unit of
work** because each new row absorbs 5-30 KB that today
contributes nothing.

## Slice 5 — Phase 4 simulation first chunk (hitbox/hurtbox)

`phase_4_simulation_boundary_v0738.md` proposed crouch;
`phase_4_crouch_boundary_v0742.md` corrected that to
hitbox/hurtbox at `+0x61e`/`+0x626` (state-25 walker
scaffolding already in place). The body is `s16(0x3) -
s16(0x7) + scaled r5 write` per
`fa_player_144b0_state25_v0395.md`. ~200 LOC + 1 ctest.

## Slice 6 — 87,284 B gap characterisation at `0x4ec00..0x640f4`

The largest unknown region per
`completion_plan_v0734.md:23`. **No `src/` change.**
Run `vf2i960 function` and `vf2i960 disasm` over the gap
to identify function extents and dispatches. Produce a
`decomp/i960/notes/gap_87kb_characterisation_v0764.md`
that names the recoverable functions and dispatches. Same
shape as `phase_1_3b_gap_characterisation_v0734k.md` but
the gap is 16× larger.

## Slice 7 — Extend recovered corridor past `0x164c4`

`native_dispatch_boundary_v0730_measured.md` measured the
proven native boundary at `0x164c4` (14,277,453 instructions,
10,288 calls / 10,286 returns on `out/sixth-fresh.vf2snap`).
Extend via a taint-driven sweep with `taint.py v1` to
identify the next dispatch target, then recover the entry
and wire a new `vf2_native_nth_dispatch` ctest entry.

## Sequencing rationale

| slice | tractability | risk | coverage delta | wall |
|---|---|---|---|---|
| 1 (recover 0x1fcc0 fully) | medium | high | 0 | ~1-2 days |
| 2 (wire 0x1fcc0 + 7 sub-callees) | small | high | 0 (live, not new) | ~0.5 day |
| 3 (siblings 17/20/21) | small | low | small (~0.005 pp) | ~0.5 day per sibling |
| 4 (4 gap runs) | small | low | **largest pp delta** | ~0.5 day each |
| 5 (Phase 4 hitbox) | medium | medium | medium | ~1 day |
| 6 (87KB gap char) | small | none (research) | 0 | ~0.5 day |
| 7 (corridor extension) | medium | low | extends boundary | ~1 day |

The dependency chain is **4 → 6 → 3 → 5 → 7 → 1 → 2**
(independence from 1+2 lets Slices 3-7 advance without
risking the wiring that has failed twice).

## What this plan should NOT claim

- **No overnight attempt at Slice 1+2.** Prior attempts
  shipped broken behaviour; the principled path is Slices
  3-7 first, then a measured attempt at 1+2 with a
  planted-defect ctest gate.
- **No ROM rewrite.** Nothing in this plan touches `roms/`.
- **No semantic naming without a second source.** Field
  names stay as `field_xxxx` until two independent
  measurements agree (per AGENTS.md).

## Validated

- `master` at `aa928e44` (v0763 retraction), fully synced.
- `git status`: one deletion to stage (`tests/recovered/
  test_per_step_loop_unsupported_pop.c`) — staged in the
  cleanup commit.
- Both prior retraction notes (`v0761` and `v0763`) are
  preserved; this plan documents them as the boundary.
