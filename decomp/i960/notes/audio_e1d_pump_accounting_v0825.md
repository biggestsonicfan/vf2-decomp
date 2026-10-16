# E1d: pump accounting follows the door (9/10 arc)

Status: three-line intent fix + doc/comment hygiene. No stepper
run (nothing new to measure); proof is the unchanged live pins
(0/3) plus the T2/E1c differentials the new pin cites. Full
suite in the commit validation.

## The ripple

E1b moved the effects-applied refusal 0xc68 -> 0xcb2 and E1c
added the OK completion, but `vf2_session_pump_audio` still
counted `UNSUPPORTED && handler == 0xc68` as dispatched -- a
pin no path returns anymore (grep: only comments). Any future
armed in-session 90 dispatch would have counted refused.

## Fix (`src/app/session.c`)

Count `VF2_OK || (UNSUPPORTED && handler == 0xcb2)` as
dispatched. Deliberately NOT 0xd92: that refusal is
unmeasured (no S arrival exists), so fail-closed leaves it
refused. Host consumers use the report for logging only.

## Doc hygiene (same slice, all stale pins)

- `include/vf2/session.h` report doc: 0xc68 -> 0xcb2.
- `test_session_audio.c` pump-leg comment: "round-1 90-class
  admitted at the engine door" contradicted the CHECKs below
  it (0 dispatched / 3 refused) -- the 90 entry misses
  unarmed; the admitted shape lives in T2, never staged here
  (pump contract: "nothing is staged or seeded").
- `docs/UNCOVERED_BRANCHES.md`: "engine past 0xc68" ->
  dead copy loop + unmeasured 0xd92 + a0 body.

## Why no armed in-session leg

The pump contract forbids staging, and arming (voice0/152e/
table bit) is staging. The engine-reaching shapes are proven
at the board level (T2 round-1: 0xcb2 + effects, C==S; E1c:
OK + active0=1 + peak 4251). The pump maps those outcomes;
it does not re-prove them.
