# 8e: session->host audio plumbing (v0825)

Status: wired and stable. Session attach + render determinism +
host-ring round-trip + sized WAV artifact, all green. Silence is
the honest ceiling (no producer); this slice proves the PIPE,
not the music.

## Leg (test_session_audio.c + vf2_session_audio_plumbing)

- Session opens on real ROMs from sixth-fresh (skip 77 without
  the gitignored snap, uart-probe precedent); audio attaches.
- Two 735-frame renders (one video frame each): OK +
  bit-identical (determinism gate).
- Host ring round-trip through the SDL shell's exact
  push/read calls (1470 frames via a 1470-cap ring):
  deinterleaved halves equal both renders (lossless gate).
  Side effect: the ring API's first direct test.
- `out/session-audio.wav`: header sizes COMPUTED
  (RIFF=36+2940, data=2940; S4b's hardcoded header corrected
  there, computed here from the start), disk-verified
  (2984 bytes, fields match, peak 0).
- Peak 0 reported, expected, not gated. Gating silence would
  launder the missing producer into a pass; gating
  determinism locks the pipe so the day voices key, the WAV
  changes for the right reason.

## What 8/10 still lacks (bridge + naming)

- Live packets (8c/8d hunt: writer ungated-unreachable).
- V3b voice naming (labels for the authored slots).
- With those, this pipe carries music unchanged.
",
