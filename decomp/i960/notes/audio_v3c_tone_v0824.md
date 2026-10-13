# v0824 - V3c: first audible render (synthetic 689 Hz square)

Status: `vf2_sound_board_render` proven end-to-end on a synthetic
voice: 512 frames @44.1 kHz, stereo, peakL 13212 / peakR 12387,
`out/first-tone.wav` (gitignored) independently verified --
period exactly 64 frames (689.0625 Hz), attack ramp
3303/6606/9909/13212 (envelope +8192/frame exact), decay onset
exact. ROM-free synthetic leg (no oracle for render); the
audibility asserts are the gate, the WAV is a bonus artifact.

## Fixture (test_sound_board.c, ROM-free suite)

Slot 0: control KEYONB|PCM8B|forward-loop (0x0830), start 0,
loop 0..64, attack 31, level bypass (0x0100), pitch word 0x0000
(= 1 sample/frame: octave field 0 gives shift +2, 0x400 << 2 =
0x1000), pan 15. Sample ROM: 64-byte 8-bit square (+100/-100,
no libm). Pins: attack ramp (frame0 < frame4 both channels),
late peak > 8000 both, mean-square > 9e6, slot still active.

## Ordering note (caught live)

First version pinned frame-0 ~= 0 (envelope 0). It failed: the
model advances the envelope BEFORE scaling within a frame, so
frame 0 sounds at 25% (3303). Neither order is silicon-proven
(no render oracle exists), so the leg pins the order-robust
fact (ramp-up) instead of one ordering's absolute. Documented
in-test.

## What this does and doesn't prove

Proves: sample ROM -> slot fetch (8-bit) -> interpolation
(exact at integer steps) -> envelope (attack/decay/sustain
arithmetic) -> pan mix -> clamp -> PCM16 render. The whole
tail end is arithmetically self-consistent.
Does NOT prove: real SCSP envelope rates/pitch (synthetic
registers), real sample data/timbre (needs mpr banks), or that
any ROM flow ever keys a voice (V3b's job).

## Remainder

- V3b voice-start C recovery (0x9ba/0xa8c arms) + trigger path.
- 0x49e `$1526` dance question.
- Real-data render (mpr banks absent locally).
