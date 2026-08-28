# `fa_game_info` mixed negative state-4 native recovery (v0148)

The previous dispatcher kept every negative-threshold mixed state-4 pair on the interpreted fallback, even when both `fighter + 0x1a4` flag words were clear. ROM-backed probing shows that this zero-flag family follows the already recovered C body exactly.

The native admission is intentionally narrow: exactly one fighter may be state 4 (the peer may be another state), both `+0x1a4` flag words must be zero, and the shared threshold at `0x0050a028` must be negative. Bilateral state-4 and flagged state-4 families retain their existing separately measured predicates/fallbacks.

Across 96 differential fixtures covering state 4 paired in both orientations with states 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 15 and 255, with both countdown values and both mode-bit-6 values, the native task reaches `0x00010dcc` with exact CPU and mutable-memory equality.

The measured mixed-state correction is: one fewer task instruction than the generic conditional accounting; final condition `EQUAL` when countdown is zero and `LESS` when countdown is nonzero; and the historical saved-frame `r3`/`r7` values remain `0x41000000` before RET.

No ROM bytes or snapshots are committed.
