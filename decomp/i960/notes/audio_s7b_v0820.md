# v0820 - S7b: e0 differential green; CMP/SUB prints unrotated

Status: first C-vs-stepper audio differential green
(`test_e0_differential`: $1524/chw/timer/SCSP all bitwise equal,
204 steps, voice match + tail both sides). Two decoder print maps
fixed (CMP, SUB — execution was already correct), three C
semantics fixed (bit-8 rule, stale removal, byte matches ×2),
v0792 hardware suite re-pinned to ROM truth (green). `src/`
changed (decoder prints, sound_board C). No oracle change.

## The differential (test_cdispatch.c)

C dispatch_next (ring cmd e3/payload 05, descriptor $4000 ch 3,
voice0 slot-0 match) vs ROM head-entry 0x12b4 (a3 stream mirrors
ring bytes, a0 = descriptor, $150e filed). Shared contract:
$1524, $4206 (ch word), timer $201a, SCSP[$10], report
channel/matched/handler. Excluded by construction: cursor/count
consume (C advances, ROM head doesn't traffic), $150e write,
side-local regs/counters. Bare-SCSP both sides (initialize FAILS
on the real 0x80000 region — proven on synthetic ROM only; which
table read falls outside is a separate probe, noted in-test).

## CMP/SUB print rotation (decoder bug, execution always right)

Motorola opmode bits 8-6: 0=B,1=W,2=L. The decoder printed
0->w,1->l,2->b for CMP and the equivalent rotation for SUB (the
ADD class was fixed pre-merge; OR was always correct). The
stepper maps 0/1/2->B/W/L correctly, so every "mismatch" below
was print-vs-execution, caught only because the differential
compared EXECUTION. Fixed with byte vectors; three existing
test vectors pinned the rotated prints and were corrected
(0xB412 cmp.b, 0xB042 cmp.w, 0x9682 sub.l — comments already said
the right sizes in two of three cases).

## Cascade: 0x1332 is cmp.B (the voice-match fix)

ROM 0xB22C bits 8-6 = 000 = BYTE: the e0 scan matches the single
voice[4] byte, not a word. Evidence chain that hid it: rotated
print (cmp.w) -> v0792 word-compare C -> green tests asserting
word behavior -> my S7b fixture ([0,3]) missed on silicon while
the C matched. The stepper was right throughout (my fixture was
wrong, not the oracle). Fixed:
- C e0 match: `voice[4u] == channel` (voice[5] ignored, as ROM).
- C clear path (0x1e1c is also cmp.B, verified): same fix.
- v0792 fixtures: tail voices [4]=2, clear voice [4]=1 (byte
  matches); G1 marker moved to the negated index 0xfd (selected
  $5a, signed -$5a); G2 stale asserts to +$0005; picker/timer
  stages moved to the +0x4a slide-0x25 offsets.
- New C truth: no stale term (0x12ba/0x12c0 andi.l are LONG,
  byte-verified: uppers zeroed entry-independently), bit-8
  negation, byte match. The v0795a stale note is retired in the
  test comments; v0792's formulas need the same pass (pointed
  here, not rewritten).

## Scan matching, production reading

With byte semantics the scan matches voice[4]==filed&$f. Boot-
authored voices carry [4]=0 (cleared) unless a path stores a
channel byte there — the clear path zeroes [4] on kill, the e0
tail never writes it. So in steady state NOTHING matches and the
scan is miss-all by construction (both S7a legs). Who WRITES
voice[4] = channel, ever? Open (S1 authorship remnant). If
nobody does, the voice tail is production-dead and the e0 path
is head-stores only. Flagged, not resolved.

## Remainder

- init-on-real-ROM failure (which table read falls outside):
  separate probe (blocks nothing: differentials use bare builds).
- Default-arm C fix + ring-consumer mapping: still S7b-deferred
  (now S8-next). The 0x90/0x80/c0 arms: unmapped paths.
- A0 stale-a2 join: still refused (untouched).
- Audibility chain: V1 voice re-recovery (R5/kazuyette's lane —
  coordinate before touching maintain_voices), V2 sample upload,
  V3 render+WAV probe.
