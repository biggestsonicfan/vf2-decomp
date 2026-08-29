# v0174: recover positive state-8 bit-6 8140/10140 composites

Entry `0x0001645c`, both fighter state bytes 8, measured matrix distribution, nonnegative shared threshold.

Fresh ROM-backed full-dispatch measurements cover all 22 remaining pair/triple/quad combinations of high bits 26/29/30/31 over the `0x8140` and `0x10140` low families (36 fixtures per mask).

## 0x8140 composites

All eleven masks share the isolated/single-family postconditions:

- instruction accounting is lower by 3 for unilateral distributions and 5 for bilateral;
- final compare state is countdown-derived `EQUAL`/`LESS`;
- the stale local frame receives the measured `0x41000000` / `0x07800f0f` register pattern.

Admitted masks: `0x24008140`, `0x44008140`, `0x84008140`, `0x60008140`, `0xa0008140`, `0xc0008140`, `0x64008140`, `0xa4008140`, `0xc4008140`, `0xe0008140`, `0xe4008140`.

## 0x10140 composites

Six masks were already exact through the existing recovered/ROM-child path and remain untouched: `0xa00010140`, `0xc00010140`, `0xa40010140`, `0xc40010140`, `0xe00010140`, `0xe40010140`.

Five masks need explicit dispatcher postconditions. `0x24010140`, `0x60010140`, and `0x64010140` add 3 unilateral / 6 bilateral instructions; `0x44010140` and `0x84010140` add 4 / 8. All five also restore countdown-derived compare state, the measured stale-frame pattern, and OR bit 11 into the selected fighter `+0x1a4` word. The three masks containing high bit 29 additionally write `0x1e` to selected fighter `+0x6da`, matching the previously recovered `0x20010140` single-high behavior.

All corrections are exact-mask guarded; unrelated masks remain fail-closed.

## Verification

The completed dispatcher block was configured and built in Release with `VF2_BUILD_TESTS=ON` and `VF2_WARNINGS_AS_ERRORS=ON`; the complete CTest suite passed before the source commit.

Post-commit CI for `3c544786ac193c416f4a7a6b2c4040fbe0007ee2` passed Python tooling syntax, GCC Release, Clang Release, and Clang ASan/UBSan, including each job's test suite. The GCC artifact from that exact commit was then run against the fresh VF2 ROM dump through the complete full-dispatch matrix for all 22 masks: **792/792 exact** (`22 masks x 36 fixtures`). The comparison includes the full snapshot, architectural signature, instruction/call/return counters, and interrupt counters.

This closes the `0x8140` / `0x10140` pair/triple/quad high-26/29/30/31 frontier left by v0173.
