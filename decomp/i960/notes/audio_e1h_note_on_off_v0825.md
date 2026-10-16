# E1h: audible note on->off, native end-to-end (9/10 arc)

Status: `test_note_on_off` green in-file (whole binary, 0
FAIL). No `src/` change -- both dispatches exist; the proof
is the composition (E1f-d precedent). Full suite in the
commit validation.

## Composition (one board, one slot)

- ON: E1c shape verbatim (0x9a packet, S4 fixture, staged
  bit31) -> ed8 keys slot 0 (KEYONB, active 1), render P1 =
  4251 peak (reproduces E1c; determinism re-pinned).
- OFF staging: fresh {0x80,0x05} packet, desc0 = LOW
  0x80/0x00/0x03/0x07 (sel-3 walk to 0x008a/0xa7), descs
  1..19 gate-shut, voice1 = slot 0 / +2 0x40 / +0xb 0.
- OFF: teardown-normal (v1+2 = 0, SCSP[0] = 0x10 stored over
  0x18, [0xb] = 0x1f, consume, cursor 4->8). S drains
  0x7a6 -> 0x265c in 518 steps (print-first) with full-RAM
  (drain exclusions) + full-SCSP equality.
- Renders: P2 = 2530 peak / last frame 0; P3 = 0 (second
  block fully silent); determinism on the release block.

## Two corrections found by measurement

1. Voice slot words are init-authored (index*0x20), NOT
   zero. First draft left voice1's slot at 0x20, so the
   teardown keyed off an idle slot while slot 0 droned
   (regs 0x18 stuck, active 1, release peaks CLIMBING:
   p2 = 14043, p3 = 16491 -- the still-keyed slot's natural
   envelope). The C==S differential stayed green throughout
   (both sides share the SCSP model); only the targeted
   slot asserts + renders caught it. Staging v1 slot = 0
   fixed it. Lesson: on an initialized board no voice field
   is zero by default -- the E1f/E1g zeroed-board fixtures
   don't transfer silently.
2. Key-off is RELEASE, not an active edge. active stays 1
   both sides after the teardown (synchronize_slot: !KEYONB
   && active -> RELEASE state; active clears only when the
   envelope bottoms out). Silence is an envelope fact
   (P3 = 0), not a slot-state fact. The "audible middle"
   caveat from E1f-d is now closed the right way: the 0x80
   class never keys (note-off driver), and the full
   key->release->silence lifecycle is native across the two
   classes.

## Arc status

E1b (slot program) + E1c (key-on) + E1f-a/b/c/d (0x80 row,
scan, sub, L1) + E1g (LOW walk) + E1h (audible on->off):
the one-shot voice lifecycle is native end-to-end with
sound coming out and going silent on command. Remaining
engine-adjacent: the 0xd92 arm (gate +0x0c word != 0,
unmeasured -- needs its own staged-gate leg) and the a0
body (residue-dependent by measurement).
