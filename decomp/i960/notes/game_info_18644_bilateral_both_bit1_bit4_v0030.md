# `fa_game_info` `0x18644` bilateral state8+bit1+bit4 corridor

## State identity

The exact combined state recovered here is `0x00000112` on both fighters:

- state bit 8: `0x100`;
- state bit 1: `0x002`;
- state bit 4: `0x010`;
- combined: `0x112`.

This is intentionally distinguished from `0x114`, which is state8+bit2+bit4 (`0x100 + 0x004 + 0x010`) and is not the bit1+bit4 composition.

The state source is the real fighter word at `fighter+0x1a4`; `0x18644` reloads the two words on entry.

## ROM reference

Using the natural `0x164ac` caller snapshot and modifying only the two fighter state words, countdown byte and mode bit 6, the pure-ROM caller-to-task totals are:

| countdown | mode bit 6 | instructions to `0x10dcc` |
|---:|---:|---:|
| 0 | 0 | 224 |
| 0 | 1 | 229 |
| 1 | 0 | 224 |
| 1 | 1 | 229 |

The corresponding final cumulative instruction counts are `14275980` with bit6 clear and `14275985` with bit6 set.

## Recovery

The C recovery admits only the exact `0x112/0x112` state in the mixed-state guards and measured threshold corridor. Other unmeasured combinations remain fail-closed.

The initial semantic candidate matched complete CPU/mutable-memory state 4/4 and matched procedure-call/return counters, leaving only instruction accounting differences.

Measured accounting corrections were:

- first `0x18644` return site `0x164b0`:
  - `+5` with zero countdown;
  - `+19` with nonzero countdown;
- second `0x18644` return site `0x164c4`:
  - `+1` with zero countdown and mode bit 6 clear;
  - `+2` with zero countdown and mode bit 6 set;
  - `+18` with nonzero countdown, independent of mode bit 6.

## Final chained differential

Acceptance used both `0x18644` invocations natively in one `fa_game_info` path:

`C child #1 -> caller ROM -> C child #2 -> caller tail ROM -> 0x10dcc`.

All four controlled combinations matched the pure-ROM reference exactly:

| countdown | mode bit 6 | final instructions | calls | returns | result |
|---:|---:|---:|---:|---:|---|
| 0 | 0 | 14275980 | 10249 | 10248 | MATCH |
| 0 | 1 | 14275985 | 10249 | 10248 | MATCH |
| 1 | 0 | 14275980 | 10249 | 10248 | MATCH |
| 1 | 1 | 14275985 | 10249 | 10248 | MATCH |

`compare-snapshots` reported `Snapshots match.` in every case, and instruction/call/return counters were verified explicitly.

## Promotion

The fully accounted recovery was promoted from commit `608344a612092f6d9cf731c10c6e2ec63a921b8a` to `master`. The temporary staging workflow and transport PRs were removed/closed after validation.
