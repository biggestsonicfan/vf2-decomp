# v0791 - channel-match + 90-head fidelity (code + fixtures)

## Code fixes (all raw-verified against the drain / 0x90 head)

1. **Descriptor match is channel-only.** Drain 0x7e6
   `cmp.w 1(a0),d0` runs with D0.W = command & 0x0f (0x7d2
   `andi.b #$f`), so descriptors match desc[1..2] ==
   0x00channel. C matched the full command byte. Every
   command-descriptor fixture is now laid out ROM-true
   (desc[1] = 0, desc[2] = channel); bodies that overwrite
   desc[2] (c0) or read it as selector (90) are unaffected in
   shape, only in fixture values.
2. **90 base**: ROM 0x85c-0x860 `move.b 4(a0),d3; add.w d3,d0`
   = payload + desc[4]-BYTE, no 0x7f mask. C masked and added
   a 16-bit word. (This also retracts half of the v0790
   "80-key" note: 80's `andi + add.w word` matches C's 80;
   the remaining 80 divergence is command-vs-payload keying.)
3. **90 LOOKUP_MODE index**: ROM 0x866/0x872 reads
   table_60c6a6[ring byte 2], skipped when zero. C indexed by
   payload. `dispatch_90` takes a new `ring_byte2` parameter
   (same pattern as a0's ring[cursor+2] argument).

## Fixture conversions (test_sound_board)

- All desc[2] = command → channel (c0/a0/e0/b0/80/D0 legs).
- 90 scenarios: selector is now channel 0 → low-path row idx 0
  (row pointer moved 0x53ea → 0x53aa, same record content);
  packet byte 2 carries the mode index (B/D packets), payload
  and byte 2 deliberately differ in scenario B so the mode
  gate can fail; B's desc[4] = 0x60 + raised limit so the
  base gate can fail (old formula runs away to OOB).
- 0x29/0x2a/0x2b legs: under uniform channel-match the
  dispatch descriptor itself matches the voice scan, so ROM
  updates it too — `descriptors_updated` 1 → 2 with
  first-descriptor store assertions (0x4a / 0x02 / 0x5a).
  The old singleton count encoded the complementary
  wrongness (command-match isolating the dispatch desc).

## Gates proven (each fails, reverted green)

- match → command: 93 failure lines across all dispatch legs.
- mode index → payload: LOOKUP_MODE assertion only.
- base → old formula: B-leg OOB runaway (5 assertions).

## Tier-2 remaining (unchanged from v0790 §Tier-2, minus 90)

Channel-match, 90 base/mode, 0x29-scan consistency: DONE.
Still open: A0 byte shift (row by command, max/ptr by byte 1),
80 key (payload&0x7f, not command), B0/E0 d0 = ring byte 2
per-handler audit, high-path reachability paradox, sequence
packet bytes (B0 raw-vs-masked, 0x80 one-fetch, stale 2-3).
The v0790 note's Tier-2 list stands otherwise.

## File map

- `src/hardware/sound_board.c`: match, 90 base, 90 mode-index
  + `ring_byte2` parameter.
- `tests/hardware/test_sound_board.c`: fixture conversions +
  differentiated payload/byte2 + nonzero desc[4] + dual-match
  store assertions.
