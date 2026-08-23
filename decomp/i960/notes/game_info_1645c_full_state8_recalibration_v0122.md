# `fa_game_info` `0x1645c`: full state-8 dispatcher recalibration and the bit-14/bit-16/high-21+26+30 composition

## Summary

Two related results, both measured with
`decomp/i960/tools/validate_game_info_full_dispatch.py` against the calibrated
`0x1645c` task-entry snapshot that the tool rebuilds deterministically from
`native-fifth-dispatch`:

1. **Recalibration of the five previously admitted full-dispatch state-8
   compositions.** Field masks `0x04214000` (high-26), `0x20214000` (high-29),
   `0x40214000` (high-30), `0x80214000` (high-31) and `0x64014000`
   (high-26+29) had been admitted using dispatcher-accounting constants and
   condition-state postconditions derived under an earlier ad-hoc fixture
   lineage. Under the committed validator's calibrated entry those rules do
   not reproduce: every case failed with raw-pattern instruction deltas and a
   wrong final condition state while memory stayed equal. All five are now
   re-measured and re-admitted under the shared rule below.

2. **New composition.** The bit-14+bit-16 triple-high field mask
   `0x44214000` (bits 14, 16, 21, 26 and 30) is now admitted through the full
   dispatcher for fighter-0-only, fighter-1-only and bilateral distributions,
   countdown `0/1`, mode-byte bit 6 clear/set and thresholds `0..2`.

All six masks pass their complete 36-case matrices exactly: final CPU/memory
snapshots equal at `0x00010dcc`, architecture signatures equal, and
instruction/call/return/interrupt counters equal in every case.

## Measured shared rules

With `m6 = mode_byte & 0x40`, `cd = countdown byte != 0`:

- **Dispatcher accounting** (identical for all six masks):
  - unilateral distributions (fighter-0-only or fighter-1-only): `+3 + m6`
    instructions;
  - bilateral distribution: `+5 + 2*m6`;
  - neither term depends on the countdown byte.

- **Condition-state postcondition**: all three distributions leave
  `compare_result EQUAL` for `cd == 0` and `LESS` for `cd != 1` (`AC` cc bits
  `0x2`/`0x4`). The isolated high-26 mask `0x04214000` already leaves this
  state naturally from the recovered child and needs no correction; the other
  five set it explicitly.

- **Stale historical frame** (`local_frames[local_frame_depth + 1]`, depth 2
  at the tail, index 3 in the final snapshot): the ROM child's deepest call
  leaves `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`. For masks
  `0x04214000..0x64014000`, every distribution that touches fighter 1 also
  leaves `r15=1`; fighter-0-only cases leave r14/r15 untouched. The
  high-21+26+30 mask uses a distribution-dependent pair instead:
  fighter-0-only leaves `r14=8, r15=0`; the other distributions leave
  `r14=0, r15=1`. Without this postcondition the C comparator still passes
  (it skips frames beyond `local_frame_depth`) but the raw-file architecture
  signature differs, so the validator correctly reports `arch=DIFF`.

## Fail-closed controls

- Unadmitted neighbour `0x60214000` (21+29+30): 0/12 exact — refused.
- Threshold `3` (outside the measured `0..2` range): 0/12 exact — refused.
- Sibling validators unchanged: child-level state-8 matrix 96/96 exact,
  state-4 matrix 192/192 exact, ctest 51/51.

## Provenance note

The superseded constants ("shared bit-14/bit-16 join": fighter-0
`+2*mode6 + cd − mode6*cd`, fighter-1 `−4*mode6 − 4*cd + 4*mode6*cd`,
bilateral `+1 − 2*mode6 − 3*cd + 3*mode6*cd`, NONE/EQUAL distribution-split
condition states) reproduced reference behaviour only against reference
snapshots generated from a different fixture lineage (see the superseded
`game_info_18644_full_state8_bit14_bit16_high26_29_v0121.md` claim). Stored
snapshots from that lineage differ from the calibrated pipeline by tens of
thousands of instructions and hundreds of calls, so they measure a different
boundary. The committed validator is now the single measurement harness for
this corridor and every admitted mask is proven against it directly.
