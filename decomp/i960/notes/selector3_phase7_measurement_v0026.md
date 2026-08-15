# Selector-3 phase-7 measured corridor (v0.0.26)

The controlled ROM corridor for selector 3 phase 7 (`0x0000b66c`) executes **90,565 instructions / 13 calls / 14 returns** from the framed `0x0000a6c0` entry to the wrapper return.

The dominant child is the already recovered display-profile apply path at `0x1fcc0`: with its outer call it contributes 90,373 instructions / 9 calls / 9 returns. The phase also invokes `0xd918`, then performs descriptor and flag updates before returning through `0x1344`.

Commit `6dd154b28a56df4634f795d6d60d9091fd9611f0` composes the phase from the existing recovered profile-apply semantics plus the observed pre/post-profile stores. It also fixes the `0x0050a00c` handoff to store the value loaded from `source + 4`, matching the i960 `ld 4(source)` instruction rather than storing the address itself. The synthetic bridge fixture now writes a known word into its `main_data` buffer and verifies the dereferenced value.

Full native-vs-ROM snapshot equality remains the acceptance criterion before this phase is considered closed.
