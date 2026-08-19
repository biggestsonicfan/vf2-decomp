# `fa_game_info` `0x18644`: legacy accounting regression audit

The reusable full-chain validator exposed stale `recovered_exact=yes` claims in five ordered state pairs. CPU state, mutable memory, calls and returns still matched the ROM; only `executed_instructions` had regressed.

## Affected pairs

- `0x100 / 0x100`
- `0x100 / 0x102`
- `0x102 / 0x100`
- `0x102 / 0x102`
- `0x110 / 0x110`

The first three shared the same per-call pattern. `0x102/0x102` and `0x110/0x110` required their own exact call-site corrections.

## Why full-chain deltas were insufficient

`decomp/i960/tools/diagnose_game_info_pair.py` was added to compare the first (`return 0x164b0`) and second (`return 0x164c4`) native child invocations independently against the same pure-ROM reference.

For example, `0x100/0x100`, countdown clear, mode bit 6 set, had a full-chain `+1` mismatch that was actually composed from `-4` at the first child and `+5` at the second child. Applying a global `-1` correction would therefore have hidden the real accounting error.

## Exact correction table

For `0x100/0x100` and `0x100<->0x102`:

- first call: `-1` for `cd0/m0`, `+4` for `cd0/m1`, no correction for countdown;
- second call: no correction for `cd0/m0`, `-5` for `cd0/m1`, `-1` for countdown.

For `0x102/0x102`:

- first call: no correction;
- second call: `-4` for `cd0/m0`, `-3` for `cd0/m1`, `-1` for countdown.

For `0x110/0x110`:

- first call: `+3` for `cd0/m0`, `+8` for `cd0/m1`, `+8` for countdown;
- second call: `+4` for `cd0/m0`, `-9` for `cd0/m1`, `+7` for countdown.

These corrections are guarded by exact state equality and return address and do not broaden semantic acceptance.

## Validation

Candidate `87429a954eab7f71ef129e3af5603fd78c610e10` passed the normal CMake/CTest staging build and the ROM-backed full-chain validator:

- affected states: 20/20 exact cases across the five ordered pairs;
- sentinels outside the correction predicates: `0x102/0x110`, `0x112/0x112`, and `0x114/0x114`, 12/12 exact cases;
- every validated case matched CPU/mutable memory plus instruction, call, return and interrupt counters.

The other previously exact state pairs are structurally excluded by the new exact-state predicates; they were verified exact immediately before this accounting-only patch.
