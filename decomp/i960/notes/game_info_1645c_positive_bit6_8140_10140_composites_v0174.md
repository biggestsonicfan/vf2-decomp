# v0174: recover positive state-8 bit-6 8140/10140 composites

Entry `0x0001645c`, both fighter state bytes 8, measured matrix distribution, nonnegative shared threshold.

Fresh ROM-backed full-dispatch measurements covered all 22 remaining pair/triple/quad combinations of high bits 26/29/30/31 over the `0x8140` and `0x10140` low families (36 fixtures per mask).

For every `0x8140` composite, architectural state already matched the sequential i960 reference and only instruction accounting was high by 3 for unilateral distributions and 5 for bilateral. v0174 subtracts exactly that measured excess for the eleven exact masks.

Six `0x10140` composites were already 36/36 exact and remain untouched: `0xa00010140`, `0xc00010140`, `0xa40010140`, `0xc40010140`, `0xe00010140`, `0xe40010140`.

The five remaining `0x10140` composites also had exact architectural state, calls, returns and interrupt counters. Their instruction-only deficits were:

- `0x24010140`, `0x60010140`, `0x64010140`: +3 unilateral / +6 bilateral
- `0x44010140`, `0x84010140`: +4 unilateral / +8 bilateral

The implementation adjusts only `native_instructions`; it deliberately does not rewrite memory, registers, condition codes or stale frames. All other masks remain outside this exact admission set.
