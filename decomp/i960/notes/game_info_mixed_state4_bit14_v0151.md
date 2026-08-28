# Mixed negative state-4 bit-14 recovery (v0151)

## Scope

v0151 extends the native `fa_game_info` recovery for mixed state-4 pairs at a negative shared threshold when the combined fighter `+0x1a4` flags are exactly bit 14.

This family previously remained behind the interpreted negative-threshold fallback even though the corresponding `0x18644` child is already recovered in C.

## Evidence

Controlled snapshots start from the stable post-dispatch checkpoint and force exactly one fighter into state 4, keep the shared threshold negative, and set the combined `+0x1a4` flags to bit 14. Both fighter top-level flags carry bit 31 so the game-info path is entered directly.

The recovered child was then admitted locally and both sides were executed independently to the task return boundary `0x00010dcc`:

- ROM reference through `vf2probe`;
- recovered C through `native-resume`;
- final snapshots compared with `compare-snapshots`.

A representative `state 4 / state 0` case executes 872 i960 instructions in the reference and 871 recovered block instructions natively while producing an exact final snapshot.

Validation covered **160 exact ROM-backed differentials**:

- exhaustive matrices for other-fighter states `0,1,2,3`, including both state-4 orientations, all three bit-14 distributions, countdown 0/1, and runtime mode bit 6 clear/set: 96 cases;
- cross-state coverage for `5,6,7,8,9,10,15,255`, in both orientations and spanning distribution/countdown/mode extremes: 64 cases.

Result: **160/160 snapshot matches** across CPU state, mutable memory, calls/returns, condition state, and task exit.

## Native change

A new `mixed_negative_state4_bit14_path` predicate mirrors the already proven bit-6 mixed-state family:

- exactly one fighter is state 4;
- fighter flag distribution is within the measured matrix form;
- combined `+0x1a4` flags equal bit 14;
- shared threshold is negative.

When that predicate holds, the task uses the recovered `0x18644` C path instead of the interpreted fallback.

Unknown mixed state-4 flag combinations remain fail-closed behind the ROM fallback.
