# v0794b - 90-path roles + prefix/refuse (code + fixtures + gates)

## What

dispatch_90 rewritten for the switch-entry truth (master note):
d2 = command (0x90-class, enters 0x85a) or payload (default,
0x858 primer); second byte = payload or ring byte 2. Mirrors the
deterministic prefix (base $150f, mode $1510 with the exact long
index) and refuses at the first trap. allocate_voice_90 deleted
(triple-dead: free-scan fiction, fresh/reuse entries unreachable,
exits stale). 90 fast/slow voice-update deleted (no ROM analog;
80 keeps the helpers until v0795a).

## Proven shape (all raw-verified in the master note)

- base = (d2+desc[4])&0xff (d3.mid = 0).
- second == 0 -> 0x10e4-mid trap: refuse (mode unread).
- mode = ROM[0x60c6a6 + (((d2+desc[4])>>8)<<16) + second].
- Past mode: refuse unconditionally. Reachable shape (drain match
  word == channel) always takes the stale low arm; the high arm
  (desc[2] >= 0xf0) cannot match; 0x8d2 is a mid-instruction trap
  on desc[13] == 0 (nothing live sets it); scan/compact/kill/alloc
  are post-trap with stale exits. No 90 packet completes on ROM.
- Report: handler 0x85a/0x858, channel, selected = mode.
  $1515/lookup words are post-death: never written.

## Fixtures (all with held-packet asserts: count stays 1, cursor
   unadvanced - fail-closed evidence)

1. 0x90 second-0: base stored, mode untouched, UNSUPPORTED.
2. 0x90 prefix (0x93/0x05): base 0xa3, mode table[5], UNSUPPORTED.
3. 0x90 mid-1 (0x9f + desc[4] 0x80): mode from image+0x10000+5
   (bigger 0x20000 image), UNSUPPORTED.
4. default 0xd3 (d2 = payload, second = byte2): base 0x17,
   mode table[9], handler 0x858, UNSUPPORTED.
5. default second-0: base, mode untouched, UNSUPPORTED.
6. old 0xd3 block reworked to the same trap (d2 = 0).
   Deleted: voice-update + alloc + 0x5000-gate blocks.

## Gates (each planted, failed exactly, reverted green)

1. d2 = payload for 0x90-class: 5 FAILs (base).
2. second = byte2 for 0x90-class: 4 FAILs (mode).
3. second-0 check removed: 0 FAILs initially (image zero aliased
   the refusal!) -> poked table[0] = 0x77 in both second-0
   fixtures -> 2 FAILs. Lesson: refusal tests must poison the
   would-be-read value.
4. mode long-index -> byte-only: 2 FAILs (mid-1 fixture).
5. final refuse -> OK: 8 FAILs (status + held-packet).

## Deferred

- 80 minimal (base + refuse) + helper deletion (v0795a).
- update_voice_packed_register rides the B0 rewrite (v0795b).
- key_off survives for maintain (boundary note).

## File map

- `src/hardware/sound_board.c`: 90 rewritten, alloc deleted.
- `tests/hardware/test_sound_board.c`: 90 fixtures rewritten.
