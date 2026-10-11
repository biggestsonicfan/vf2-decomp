# v0793 - 80 key + B0 byte2 + c0 address (code + fixtures)

## 80 key (0x10d0 head)

ROM: base = payload&0x7f + desc[4..5]-word; high-path row step
(payload&0x7f)*6. C keyed both on command&0x7f.
`dispatch_80` takes payload (call site updated). Fixtures:
lookup index moves with base 1 → 0 (+0x13 seed → +0x12,
LOOKUP_CURRENT 1 → 0). Gate: base-plant fails both 80
scenarios (lookup + fast-path voice legs).

## B0 d0 = ring byte 2 (0x13ca head: fetch + peek)

`dispatch_b0` takes `ring_byte2`; value, 0x29-pack,
0x2a-shift, entry-7 scale and entry-10 table index consume it
(entry/voice-match/channel logic unchanged — d2/channel).
Fixtures differentiate every path (byte2 ≠ payload):
entry1/2 value 5, entry7 0xa1 triple, entry10 row 3 (0x1e),
0x29/0x2b pack 0x39 dual stores, 0x2a 0x79/0x01 split.
Voice-tail legs keep byte2 = 0 (tails deferred, green).
Gate: payload-for-byte2 plant fails exactly the 14
differentiated assertions across all six paths.

## c0 address

c0 enters at its fetch (0x1e5a), not 0x1e5c: same +2 print
bug as 90/80/e0/a0. Constant + test expectation fixed. Body
was already fetch-correct (d0 = payload, no peek).

## Deferred with verdicts (not changed)

- entry16: structural — ROM stores the scaled factor to
  shared 0x151f and recomputes desc[10] over 10 command
  descriptors; C writes desc[3] per inbound descriptor.
  Needs re-derivation, not a source swap.
- e0 combined/index: ROM builds payload&0x7f | byte2&0x7f<<7
  and indexes post-negation; C builds command|payload<<7 and
  indexes pre-negation. Needs the 0x1308+ tail dump +
  fixture recompute.
- voice tails + $13(a5)/$4013(a5) window: the slot word is
  used UNSCALED as a byte offset, so the C per-slot SCSP
  abstraction is shape-wrong there too. Needs the a5-window
  identity (SCSP sub-file vs RAM mirror) first.
- 80-low scan `cmp.w` vs stale-high D2.W: unmodelable
  history-dependence; clean-word reading kept, documented.
- e0 `$401a(a2)` at absolute 0x601a (ROM space): likely
  hardware-ignored; C's live 0x4200 channel-table write
  needs a verdict (kept for now).

## File map

- `src/hardware/sound_board.c`: 80 payload param + keys,
  B0 byte2 param + 5 swaps, c0 address.
- `tests/hardware/test_sound_board.c`: differentiated
  packets + recomputed asserts + c0 address.
