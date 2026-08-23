# `fa_game_info` `0x1645c`: complete state-8 bit-14/bit-16 dispatcher family

## Summary

Every remaining state-8 composition over the bit-14+bit-16 base is now native
through the full dispatcher. Together with the six previously admitted masks
and the seven pair/triple admissions of the same session, all 32 high-bit
subsets over {21, 26, 29, 30, 31} — including the empty set `0x00014000`,
the four isolated high bits without bit 21 (`0x04014000`, `0x20014000`,
`0x40014000`, `0x80014000`), all ten pairs, all ten triples, the five quads
(`0xE4014000`, `0x64214000`, `0xA4214000`, `0xC4214000`, `0xE0214000`) and
the full quint `0xE4214000` — pass their complete 36-case matrices exactly:
final CPU/memory snapshots equal at `0x00010dcc`, architecture signatures
equal, instruction/call/return/interrupt counters equal.

## Measured rules (family-wide)

Identical to the previously recalibrated rule; no per-mask variation was
observed anywhere in the family:

- unilateral distributions `+3 + mode_bit6`, bilateral `+5 + 2*mode_bit6`
  instructions; countdown-independent;
- final condition state is left naturally by the recovered tail (EQUAL for a
  zero countdown byte, LESS otherwise); no correction required;
- stale historical frame at `local_frames[local_frame_depth + 1]`:
  `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000` always, plus
  `r15=1` in every distribution that touches fighter 1 (the earlier
  high-21+26+30 admission keeps its distribution-dependent `r14/r15` pair);
- thresholds `0..2` admitted and proven; threshold `3` refuses.

## Measurement note

Before admission these compositions executed through the third dispatch
ternary branch, which runs the recovered *child* but leaves the dispatcher
accounting at the ROM-child baseline (`−2` unilateral / `−3` bilateral raw
deltas versus the reference, plus stale-frame signature drift). That baseline
hides the real dispatcher join: after routing the masks through the measured
native path, the residuals collapsed onto exactly the shared
`+3+m6 / +5+2*m6` rule already proven for the six previously admitted
compositions. The first probe batch initially suggested a second accounting
class; that hypothesis was discarded once the same masks were re-measured on
the native path.

## Fail-closed controls

- Outside-family neighbours refuse: `{14,15}` base-only `0x0000C000`,
  `{14,15}`+26+29 `0x2400C000`, `{15,16}` base-only `0x00018000` — all 0/12.
- Threshold `3`: 0/12.
- Regression: ctest 51/51, child-level state-8 matrix 96/96, state-4 matrix
  192/192, previously admitted family spot checks still 36/36.
