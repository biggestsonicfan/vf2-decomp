# Selector-3 phase-12 measured corridor (v0.0.26)

The controlled selector-3 phase-12 invocation was measured from the framed `0x0000a6c0` entry using the real post-phase-7 machine state with only the phase byte changed to 12.

ROM execution returns normally to `0x0000a010` after **33,867 instructions / 6 calls / 7 returns**. The phase executes the same descriptor-blit primitives used by phase 6: `0x8ef0`, then `0x8f1c`, then a second `0x8f1c` using descriptor `0x02a6c0da` and the profile-selected destination.

Commit `90f103c317bde64087ffb9f2e966de407a199999` reuses the proven phase-6 descriptor semantics to derive the final live register state from the descriptor itself: signed addend, word mode, rows/columns, final signed sample, and profile-selected destination. That source state matched the exact 33,867 / 6 / 7 accounting and all CPU state, but the full snapshot exposed one remaining 32-bit side effect: ROM stores the selected destination `g9` at `stack + 0xc0` (`0x005ff680` in the controlled frame).

Commit `3794be2c5bf0e5197341c178406ee1a8ac5b977d` restores that observed saved-`g9` spill. This is the same architectural spill seen in phase 6 and is now supported by direct phase-12 snapshot evidence rather than assumed by analogy.

Full native-vs-ROM snapshot equality remains the acceptance criterion for closing the phase.
