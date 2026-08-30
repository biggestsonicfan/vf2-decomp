# v0196: texture number diagnostic path

The recovered texture counter expiry helper reaches the record publisher at `0x0004b9b8`. Values greater than `0x56` do not fail or publish a record in the original i960 program. Instead, the publisher calls the diagnostic formatter at `0x000093e0`, renders the offending signed texture number plus `tex num error` into tile RAM, and returns normally.

The recovered path models the measured behavior directly:

- the invalid publisher consumes 165 instructions total; the five pre-diagnostic instructions were already accounted, so the diagnostic tail contributes 160;
- each diagnostic contributes two nested procedure calls and two returns;
- the numeric field occupies six styled tile cells at `0x01000064`; negative values reserve the first cell for `-` and right-align the magnitude in the remaining five cells;
- the literal `tex num error` occupies 13 styled cells beginning at `0x01000072`;
- the diagnostic writes 19 cells / 38 bytes, leaves the separator cell at `0x01000070` untouched, restores measured `g0`/`g9` poststate, and preserves the observed stale-frame values;
- an invalid value skips record publication, while the following `value + 1` publisher is still evaluated normally, including 32-bit wraparound.

The former upfront `argument0 > 0x56` rejection in the counter-expiry helper is removed. Each of its two publisher invocations now independently performs the original diagnostic or normal publish behavior.

## Validation

The candidate was compared against the ROM-backed i960 interpreter with full snapshots from texture-counter entry `0x0004bb98` through `0x0004bc58`.

- 10 focused values covering valid controls, the `0x56/0x57` boundary, one/two diagnostics, large positive values, wraparound, negative values, and `INT32_MIN`: **10/10 exact snapshots**.
- counter0/counter1 × seven boundary/large values × `argument1` values `0`, `1`, and `0x10`: **42/42 exact snapshots**.
- standard local CTest gate: **23/23**.
- ROM-backed `vf2_texture_bridge_differential`: **passed**.

No ROM data or generated ROM-derived binary is committed.
