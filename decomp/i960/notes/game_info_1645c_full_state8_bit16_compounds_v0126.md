# `fa_game_info` `0x1645c`: recover the bit-16 + bit-21 compounds

## Result

The two masks left retired by v0125 are now recovered:

- `0x00210000` ({16,21})
- `0x00218000` ({15,16,21})

The v0125 differential isolated a single architectural memory gap in the
recovered `0x18644` child corridor: when the active fighter record carries
either exact mask, the reference sets bit 15 of the 16-bit field at
`fighter + 0xb24` (the high byte is at `+0xb25`). The native child now
performs the same read/modify/write, preserving all other bits.

The already measured full-dispatch instruction deficits are admitted unchanged:

- `0x00210000`: unilateral `+4`, bilateral `+7`;
- `0x00218000`: unilateral `+4`, bilateral `+8`.

The admission remains exact-mask-only, state-8-only and threshold `0..2`;
neighboring/unmeasured compositions remain fail-closed. Stale-frame handling
continues through the shared compound-21 clause.

## Validation

The repository CI build, CTest suite and sanitizer suite are required to pass.
The ROM-backed 36-case matrices remain the authoritative differential gate when
a supported local ROM set is available; no ROM-derived artifact is committed.
