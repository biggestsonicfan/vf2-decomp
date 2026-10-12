# v0799d - left-explicit close-out (slice 5, docs only)

## d2-residue: documented and closed (decision, not deferred)

The ISR rebalance compares full d2.W (move.b-loaded age over
interrupt-residue high bytes) against the word at voice+9. The C
keeps the clean zero-extended age with an in-code comment. No 68k
execution oracle exists, and building a 68k stepper just for this
would create a second oracle needing its own validation for one
comparison. The residue case is unmeasurable; the clean case
matches exactly. Closed by decision 2026-10-12; reopen only if a
68k oracle ever exists.

## L3/MIDI seam: documented, no code

Production C never calls emit_command (verified: only tests do;
game.c's pump runs dispatch + L1 tick). emit_command is the
modeled equivalent of the L3 ISR ring-fill; the $1500-$1503
byte-framing protocol stays unmodeled until match flow produces
commands. No C change: adding an uncalled model would be theater.

## v0796 note supersede index (that file is untouched history)

- "maintain_voices ROM-fidelity ... untouched" -> DONE in v0797
  (ISR loop A match, V1 key) and v0799b (e0 indexed stores).
- "no static entry => dead" (driver) -> CORRECTED in v0797 (L2
  vectored, worker-forward live) and v0799c (voice-trigger mapped).
- "all sample-start code is dead" -> SHARPENED in v0799b/v0799c
  (SA from staged $5000 table + KEYON via slot+0, post-refusal,
  unmodeled; DRAM never directly addressed).
- "stream worker fiction deleted" -> UPHELD (countdown half dead:
  +0x0c never arms; self-disarms at 0x220e).
- "Voice tails' slot identity" -> settled (voice[0] = slot*0x20
  used raw everywhere; absolute forms retract-ed in v0797/v0799b).

## Frontier status after slices 1-4

- Recovered and holding: drain/switch/prefixes, boot mirror,
  L1 tick (V1 keys), e0 tail (scatter + 0x201a timer), C0/0x1de0,
  B0 0x3a upload, init/uploader, stream no-op.
- Mapped and held: 90/80-body chain (Slice 4 recipe), B0-sub
  scans, B0 table (19 targets), orphans (all dead-entry bodies),
  L2/L3 ISRs, 0x3000-loops, second wave uploader (idx-44 tail).
- Live-unrecovered (not dead): main-loop activation tails
  (0x24d2/0x260c: slot+1/+6 writes, MIDI poke, voice-bit sets).
  The C starts no voices; coherent fail-closed.
- Instrument: mode-6 print bugs (3 handlers) + ORI/EORI + AnDn
  fixed with ROM-byte vectors; ANDI/SUBI/CMPI mode-6 silence and
  move-src mode-4/abs-src else-sloppiness noted as gaps, no
  observed frontier instance pending.

## Standing rule (repeated from v0798 for the record)

68k recoveries are static-match, not differential-proven. Byte
-level ea verification per site + planted gates (which prove only
that tests can fail) are the acceptance bar.
