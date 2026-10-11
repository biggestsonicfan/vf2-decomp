# v0794a - boot init mirror (code + fixtures + gates)

## What

`vf2_sound_board_initialize` now mirrors the ROM boot inits
(0x28e SCSP, 0x320 `$1526`, 0x336 descriptor images + desc[10]
precompute + `$151f`/`$151a`, 0x3a8 voice records, 0x3cc stream
records, 0x402 volumes, 0x422/0x1ac0 sample uploader) instead of
zero-filling. Signature void -> vf2_status (fail-closed on short
ROM; game.c propagates). Shared uploader covers boot (d1<0 skip,
row 0x608df0) and later B0-0x3a (0x7f trap armed, row by payload).

## Corrections during the slice

- Wave-blob pointer is the LONG at 0x604940 (not 0x604942).
- Uploader row = be32(row_address); gate = its high word (the d1
  word and the row pointer overlap at 0x608df0).
- d3.mid == 0 PROVEN (records-file +0x17 never written; the
  record-31 "256" wraps in the +0x16 LOW byte). No phase, no
  d3mid field. Master note updated.
- `$151a` write is at absolute 0x251a (not 0x251e+0x0c).
- Stray `slot_base = 0u` removed (writes hit $5000 table).

## Fixtures

test_boot_rom (0x10010u, keeps historical 0x12/0x34 + table pokes):
structural pointers/images, voice slot words, d1 = -1 (skip
upload), vol[0] = 0x5a. Refill after every memset-wipe (15 sites).
New asserts: voice words + sample longs, stream records (incl.
record-31 wrap), $151f/$151a/$1526, SCSP consts + vol, desc[10]
compute + keep, RICH upload-ok (staging/record/slot-table/sample
bytes), guard-trip (UNSUPPORTED + partial effects), tiny-ROM
fail-closed (OUT_OF_BOUNDS).

## Gates (each planted, failed exactly, reverted green)

1. voice word -> 0: 52 FAILs.
2. desc[10] store skipped: 1 FAIL.
3. $151f store skipped: 2 FAILs.
4. $6FFFF guard neutered: 1 FAIL (guard test).
5. first-copy skipped: 4 FAILs.
6. record loop skipped: 11 FAILs.
7. stream loop skipped: 122 FAILs.
8. $151a write skipped: 1 FAIL.
9. vol loop skipped: 1 FAIL ($17).
10. $0402 value 0x138 -> 0x139: 1 FAIL.

## Deferred (unchanged this slice)

- slot = word & 31 (wrong for ROM voices; dies with the alloc
  rewrite - no live caller survives: key_off/update/allocate all
  go in v0794b/v0795).
- $400001 system-port writes (dropped, documented).
- 0x422 dance (no RAM effect, dropped, documented).
- 90/80/e0/B0/c0/A0 role corrections (v0794b/v0795).
- Sequence + PCM (v0796).

## File map

- `src/hardware/sound_board.c`: uploader + initialize.
- `include/vf2/sound_board.h`: status signature.
- `src/game/game.c`: propagate boot failure.
- `tests/hardware/test_sound_board.c`: boot-ROM helper + asserts.
