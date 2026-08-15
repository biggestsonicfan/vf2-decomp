# Selector-3 phase-7 measured corridor (v0.0.26)

The controlled ROM corridor for selector 3 phase 7 (`0x0000b66c`) executes **90,565 instructions / 13 calls / 14 returns** from the framed `0x0000a6c0` entry to the wrapper return.

The dominant child is the already recovered display-profile apply path at `0x1fcc0`: with its outer call it contributes 90,373 instructions / 9 calls / 9 returns. The phase also invokes `0xd918`, then performs descriptor and flag updates before returning through `0x1344`.

Commit `6dd154b28a56df4634f795d6d60d9091fd9611f0` composes the phase from the existing recovered profile-apply semantics plus the observed pre/post-profile stores. It also fixes the `0x0050a00c` handoff to store the value loaded from `source + 4`, matching the i960 `ld 4(source)` instruction rather than storing the address itself. The synthetic bridge fixture writes a known word into its `main_data` buffer and verifies the dereferenced value.

The first full differential run on that source state matched the exact 90,565 / 13 / 14 accounting and all CPU state, with one remaining 32-bit memory difference at `0x0050a160`. ROM tracing showed that `0x3727c5ac` is stored after the `0x1fcc0` profile apply at `0xb898`; the earlier recovered handler wrote it before the child call, allowing the child to clear it. Commit `c56f5e8759da1453b42b80504d83a516131b1f56` restores the ROM ordering by applying that handoff store in the post-profile tail.

A full native-vs-ROM snapshot comparison of the final source state is the acceptance criterion for declaring this phase closed.
