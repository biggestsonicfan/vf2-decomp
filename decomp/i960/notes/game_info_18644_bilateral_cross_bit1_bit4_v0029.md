# `fa_game_info` `0x18644` bilateral cross bit1/bit4 corridor

## Scope

This note records the ROM-backed recovery of the exact bilateral cross-state at the two `0x18644` calls made by `fa_game_info`:

- `fighter0+0x1a4 = 0x00000102`, `fighter1+0x1a4 = 0x00000110`;
- the reverse physical orientation `0x00000110 / 0x00000102`.

The state source is the real fighter state word at `fighter+0x1a4`. Synthetic edits to caller `r7/r8` are not accepted as evidence because `0x18644` reloads both words on entry.

## Boundary and harness calibration

The calibrated caller boundary is `0x000164ac`. The first native child returns at `0x000164b0`; the second caller invocation returns at `0x000164c4`; final comparison is at the task boundary `0x00010dcc`.

For direct native child execution the snapshot IP is advanced to the architectural return address without manually incrementing `executed_instructions`: `hybrid_execute_game_info_18644` accounts the replaced CALL itself through `vf2_i960_cpu_enter_procedure` and its own instruction accounting.

During this recovery the ptrace harness was corrected to use the actual `vf2_i960_snapshot_restore(snapshot, cpu, machine)` argument order. The earlier reversed CPU/machine argument interpretation caused `VF2_ERROR_INVALID_ARGUMENT` and was a harness defect, not an unrecovered ROM branch.

## ROM reference matrix

Caller-to-task totals for either physical orientation are:

| countdown | mode bit 6 | instructions to `0x10dcc` |
|---:|---:|---:|
| 0 | 0 | 208 |
| 0 | 1 | 213 |
| 1 | 0 | 205 |
| 1 | 1 | 210 |

The controlled reference snapshots were generated from the same natural `0x164ac` caller state, modifying only the two fighter state words, countdown byte and mode bit 6.

## Native recovery

The recovered C implementation admits only the exact `0x102 <-> 0x110` composition in the mixed-state guards and the measured threshold corridor. Other unmeasured mixed compositions remain fail-closed.

Instruction accounting is direction- and return-site-sensitive. The final candidate contains the measured corrections for:

- first return site `0x164b0`;
- second return site `0x164c4`;
- both physical orientations;
- countdown zero/nonzero;
- mode bit 6 clear/set.

## Final chained differential

Acceptance used both `0x18644` invocations natively in the same `fa_game_info` path:

`C child #1 -> caller ROM -> C child #2 -> caller tail ROM -> 0x10dcc`.

All eight combinations matched the pure-ROM reference exactly:

| orientation | countdown | mode bit 6 | final instructions | calls | returns | result |
|---|---:|---:|---:|---:|---:|---|
| `0x102 -> 0x110` | 0 | 0 | 14275964 | 10249 | 10248 | MATCH |
| `0x102 -> 0x110` | 0 | 1 | 14275969 | 10249 | 10248 | MATCH |
| `0x102 -> 0x110` | 1 | 0 | 14275961 | 10249 | 10248 | MATCH |
| `0x102 -> 0x110` | 1 | 1 | 14275966 | 10249 | 10248 | MATCH |
| `0x110 -> 0x102` | 0 | 0 | 14275964 | 10249 | 10248 | MATCH |
| `0x110 -> 0x102` | 0 | 1 | 14275969 | 10249 | 10248 | MATCH |
| `0x110 -> 0x102` | 1 | 0 | 14275961 | 10249 | 10248 | MATCH |
| `0x110 -> 0x102` | 1 | 1 | 14275966 | 10249 | 10248 | MATCH |

`compare-snapshots` reported `Snapshots match.` for every case. The instruction, procedure-call and procedure-return counters were also checked explicitly because snapshot comparison is not relied upon for those counters.

## Promotion

The validated recovery was promoted from candidate commit `41d4c9ffc2ac6d86635b438b81800cde09e0d017` to `master`. Temporary staging and diagnostic workflows and their transport PRs were then removed/closed.
