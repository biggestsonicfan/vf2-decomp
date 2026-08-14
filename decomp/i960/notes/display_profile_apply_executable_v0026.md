# Executable display profile apply (v0.0.26)

`0x0001fcc0..0x0001fee0` is recovered as the generic parent procedure rather than a selector-3-only snapshot. The implementation reproduces both player-state branches that can force mode 12, runtime flag bits 20/21, the mode 10/11 transitions, profile constants, table-derived video parameters, and the five direct calls to the already recovered children at `0x1ff0c`, `0x1fffc`, `0x4b410`, `0x2eab8`, and `0x11704`.

Parent-exclusive instruction accounting is accumulated instruction-by-instruction along the actual branch path. Child accounting remains compositional. The selector-3 synthetic `123638 / 15 / 15` delta is deliberately not reduced in this commit; that subtraction is reserved for the next differential checkpoint of the complete parent against the controlled selector-3 state.
