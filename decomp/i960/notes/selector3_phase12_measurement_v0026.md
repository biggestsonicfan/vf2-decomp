# Selector-3 phase-12 measured corridor (v0.0.26)

The controlled selector-3 phase-12 invocation was measured from the framed `0x0000a6c0` entry using the real post-phase-7 machine state with only the phase byte changed to 12.

ROM execution returns normally to `0x0000a010` after **33,867 instructions / 6 calls / 7 returns**. The phase executes the same two descriptor-blit primitives used by phase 6: `0x8ef0`, then `0x8f1c`, then a second `0x8f1c` using descriptor `0x02a6c0da` and the profile-selected destination.

The existing recovered phase-12 memory effects already matched ROM. The remaining differences were wrapper accounting and live register poststate left by the final `0x8f1c`: `g1`, `g2`, `g4..g7`, `g9`, `g0`, and the signed comparison condition.

Commit `90f103c317bde64087ffb9f2e966de407a199999` reuses the proven phase-6 descriptor semantics to derive those registers from the descriptor itself: signed addend, word mode, rows/columns, final signed sample, and profile-selected destination. It deliberately does not copy the phase-6 `stack+0xc0` saved-`g9` spill because phase 12 had no memory mismatch and ROM does not require that phase-6-specific wrapper side effect here.

Full native-vs-ROM snapshot equality remains the acceptance criterion for closing the phase.
