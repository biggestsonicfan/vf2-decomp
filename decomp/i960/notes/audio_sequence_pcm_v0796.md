# v0796 - sequence dead verdict + stream no-op + PCM boundary (finish)

## Sequence engine: dead, fiction removed

- Driver 0x1f16 has NO static entry anywhere in the image (full
  bcc/jsr-pcrel/jsr-abs xref): never runs.
- Every stream slot's +0x0c byte is boot-cleared and never stored
  nonzero (audited worker/appliers/tail/A0-setup/main-loop files),
  so the 0x21ba prescaler would abort to the main loop at slot 0
  regardless. Double-dead.
- Packet flow on ROM is main-CPU-authored (drain-consumed); the 68k
  never sequences. The applier byte-roles (raw/masked/stale-2-3),
  F7/F0 paths, and tail countdown words are documented from dumps
  but unreachable; FF/F0-else are trap-refusals in the same dead
  region.
- C: deleted sound_board_process_stream_descriptor (F-escape/
  timer/chain fiction, ~300 lines) and its tests; maintain_streams
  is a documented no-op (zero report + cursor stillness). game.c
  pump unchanged. Stream constants kept (API stability).

## PCM boundary (no code beyond wave probe)

- SCSP render/envelope/loop/interp are spec-written DSP (not
  ROM-measured): boundary, unchanged.
- read_sample routes ram-if-attached else rom else silence-0.
  Nothing attaches sample_ram, so $50000-uploads are staged but
  unaddressed: no live path programs sample-starts (all such code
  is dead), so wiring ram would change nothing observable. Not
  wired; documented. Deliberately NOT "fixed": attaching ram would
  shadow sample_rom at low addresses and break ROM playback with
  zero evidence for HW arbitration.
- Hardened: wave-blob copy (SCSP+0x700) now probed nonzero (0x5a)
  in the helper image with assert + plant.
- Render on boot state covered by the existing 1-frame test.

## Gates

1. wave-copy skipped: 1 FAIL (SCSP+0x700 probe).
2. timer-decay reintroduced in maintain: 1 FAIL (armed-desc
   unchanged fixture).
(Deletions evidenced by the removed fiction tests + xref +
$000c proof.)

## Left explicit (future)

- maintain_voices ROM-fidelity (main-loop voice-update source
  unidentified; key_off slot form) - boundary note, untouched.
- Voice tails' (a5,d4.w) slot identity vs SCSP sub-file - settled
  for zero-word... no: voice[0] = slot*0x20 proven; C uses
  absolute + (a5,full-word) forms where live.
- FM/DSP, windowing, gamepad, pacing, match flow - open per README.

## File map

- `src/hardware/sound_board.c`: worker deleted, maintain no-op.
- `tests/hardware/test_sound_board.c`: stream fiction -> no-op
  fixtures; wave probe.
- `README.md` / `docs/UNCOVERED_BRANCHES.md`: status lines.
