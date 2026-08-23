# `fa_game_info` `0x1645c`: bit-14+bit-15 dispatcher family and legacy-admission audit

## Summary

1. **New measured family.** All 28 state-8 compositions over the
   bit-14+bit-15 base (`0x...C000`) are now native through the full
   dispatcher with complete 36-case matrices exact, including the previously
   stale mask `0x0020C000`. The family uses one measured per-record rule:
   every touched fighter record contributes `3 − 4*countdown + mode6`
   dispatcher instructions (unilateral applies the factor once, bilateral
   twice). Condition state stays natural; stale-frame postconditions match
   the shared family clause.

2. **Legacy-admission audit.** Every remaining pre-existing full-dispatch
   state-8 admission was re-measured under the committed validator. Eleven
   masks failed their matrices at HEAD before this change — the bit-14+21,
   bit-15+21, bit-16+21, bit-15+16+21 compounds (`0x00204000`,
   `0x00208000`, `0x00210000`, `0x00218000`) and all seven measured
   bit-14+15+16 base compositions (`0x0021C000`, `0x0421C000`, `0x2021C000`,
   `0x4021C000`, `0x8021C000`, `0x2421C000`, `0xE421C000`). Their predicates
   carried constants from the superseded ad-hoc fixture lineage. An attempt
   to re-admit them under unified or per-record rules showed genuine
   distribution-asymmetric accounting (for example mask `0x0421C000`
   measures f0-only `{3,2,2,2}` versus f1-only `{3,8,7,8}` over the
   cd/m6 matrix), i.e. structure that is not yet recovered. Those eleven
   masks are therefore retired from admission and now fail closed until a
   proper measurement recovers their real dispatcher behaviour.

## Measured rule for the admitted C000 family

With `cd = countdown byte != 0`, `m6 = mode byte & 0x40`:

```text
correction = nrecords * (3 - 4*cd + m6)
```

applied to `native_instructions` on top of the shared native-child path.
Neither term depends on threshold inside `0..2`. Verified across all three
fighter distributions × cd × m6 × thresholds {0,1,2} for each of the 28
masks: `0x0000C000`, `0x0400C000`, `0x2000C000`, `0x4000C000`,
`0x8000C000`, `0x2400C000`, `0x4400C000`, `0x8400C000`, `0x6000C000`,
`0xA000C000`, `0xC000C000`, `0x2420C000`, `0x4420C000`, `0x8420C000`,
`0x6020C000`, `0xA020C000`, `0xC020C000`, `0x6400C000`, `0xA400C000`,
`0xC400C000`, `0xE000C000`, `0x6420C000`, `0xA420C000`, `0xC420C000`,
`0xE020C000`, `0xE400C000`, `0xE420C000`, `0x0020C000`.

## Fail-closed controls

- Retired masks refuse: spot checks `0x0021C000`/`0x0421C000`/`0x2021C000`/
  `0x4021C000` all 0/12.
- Outside-family neighbours still refuse (`{15,16}` base-only, `{14,15,16}`
  base-only, threshold 3).
- Regression: ctest 51/51; child-level state-8 matrix 96/96; state-4 matrix
  192/192; {14,16}-family and b1415-family spot checks all 36/36.

## Provenance

The audit closes the last known exposure to the superseded fixture lineage:
every state-8 full-dispatch admission in the tree is now proven directly
against the committed `validate_game_info_full_dispatch.py` harness.
