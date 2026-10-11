# v0792 - stream base fix + A0 byte-shift (code + fixtures)

## Stream files are at absolute 0x3000, not 0x2000

Five independent ROM sites use 0x2000(a6)/0x2020(a6) with
A6 = 0x1000 (boot `lea $1000,a6`, never reassigned — the one
counter-reading was my own Am-field misparse, rechecked bit
by bit): driver stream heads (0x1f32 etc.), A0 >max/primary
bases (0x1eae/0x1eb0-join), alternate scan (0x1ed4), F2
64-longword clear (0x21a8: 64 × 4 B = 8 × 32 B ✓ the file
geometry), entry-0x12 stores (0x18a8 → 0x300C/0x300D).
`VF2_SOUND_DESCRIPTOR_BASE` 0x2000 → 0x3000,
`VF2_SOUND_STREAM_ALTERNATE_BASE` 0x2020 → 0x3020 (overlap
preserved: the alternate file overlays the primary in ROM
too). Likely origin of the old value: 0x2242's sample-
upload control pointer (a0 = 0x2000 from boot), a different
structure. Absolute 0x2000-0x27FF outside voices/cursors is
unassigned and unmodeled.

## A0 byte-shift fix (0x1e76 head)

ROM: row = 0x608e12 + (command&0x7f)*4; maximum vs payload
(ring byte 1, fetched 0x1e88); row pointer = payload. C used
payload for the row and ring byte 2 for max/pointer.
`dispatch_a0(board, command, payload)` now; call site
simplified (no more ring[+2] argument).
- Primary (≤max, joins at 0x1eae): desc[4] = longword AT the
  data pointer, desc[8] = pointer past it — C was off by 4
  (read data+4/data+8). Fixed.
- >max arm (0x1eae): initializes from a STALE a2 (never
  loaded on that path — entry state from much earlier
  history). No deterministic function of the command:
  fail closed with UNSUPPORTED (C silently returned OK).
  New leg pins payload > max → UNSUPPORTED.
- Alternate scan verified equivalent (desc[4] = a2 where a2
  is already post-flags-fetch = data+1, as C had it).

## Fixtures

- A0 row seeds moved idx 1 → idx 0x20 (same table content);
  primary asserts updated to the unshifted stores
  ([4..7] = 00 00 00 00, [8..11] = 00 60 80 04).
- Stream tests ride the symbolic constants (auto-moved) plus
  one new ABSOLUTE leg (0x3000 active / 0x2000 untouched) —
  without it the base constant is uncatchable, as the plant
  run demonstrated (0 failures).

## Gates proven (each fails, reverted green)

- A0 row key +1: both A0 scenarios fail.
- A0 primary off-by-4 restored: exactly the two store asserts.
- Stream base → 0x2000: exactly the two absolute asserts
  (and nothing else — proving the old suite was base-blind).

## Tier-2 remaining

80 key (payload&0x7f), B0/E0 d0 = byte 2 per-handler audit,
high-path paradox, sequence packet bytes (B0 raw-vs-masked,
0x80 one-fetch, stale 2-3), e0 `$401a(a2)`-at-0x601a store
(new: ROM writes ROM-space = likely hardware-ignored; C
models a live channel-table write at 0x4200 — needs a
verdict), PCM hardening (formats/envelope/end-to-end).

## File map

- `include/vf2/sound_board.h`: two base constants + comment.
- `src/hardware/sound_board.c`: A0 signature/row/max/ptr/
  primary-shift/>max-refusal.
- `tests/hardware/test_sound_board.c`: A0 row re-seed +
  unshifted asserts + >max leg + absolute base leg.
