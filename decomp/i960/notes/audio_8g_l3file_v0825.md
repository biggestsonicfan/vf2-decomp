# 8g: L3 receiver byte-file recovered (v0825)

Status: `vf2_sound_board_file_uart_byte` + 9-leg synthetic
differential green (full-RAM C==S, regs, SCSP, step pins, poke
event pin, designed 0xff refusal). The synthetic chain
(queue -> bytes -> ring -> voices -> WAV) is now composable
entirely from stepper-proven C. Full suite in the commit
validation.

## Layout correction (load-bearing)

The ring assembly destination is **0x1000 + cursor**, not
0x2500: `lea $0000(a6),a2` with a6 = 0x1000. The $1500 page
holds STAGING ($1500/$1501 filed bytes, $1502 age, $1503 file,
$1504 write cursor, $1506 count); the assembled entries land in
the 0x1000 command ring the drain consumes (shared write
cursor with emit_command). First draft filed into 0x2500 and
the differential caught it immediately (S ring[0] = source
byte, C zeros). New constant: RING_STAGE_BASE 0x2500;
reused COMMAND_RING_BASE / COMMAND_WRITE_CURSOR /
COMMAND_COUNT.

## Arms (all in the C, all pinned)

- 0xff: reboot door (sr mask, $401 clear, a7 reset, bra $100)
  refused cleanly; S pinned at the 0x5ec door in 2 steps.
- >= 0xf0: pure ignore (branches over the $1503 file; zero
  state change, zero events). 5 steps.
- bit7 set: $1500 file; d1 = 2 (1 when 0xc0 <= b < 0xe0);
  $1502 file; $1503 epilogue. 14/17 steps.
- bit7 clear: d1 = $1503 (or $1502); ring iff d1 == 1 BEFORE
  the subq (the beq tests d1 - 1 == 0 -- first draft had this
  backwards and the differential caught it: S filed the ring
  while C filed $1501); else $1501 file; $1503 epilogue files
  the POST-sub value (ring path files 0, not 1).
- Ring A ($1502 != 1): word-copy staging + byte; ring B
  ($1502 == 1): byte-copy + byte. Cursor += 4 (& 0xfff, wrap
  leg pins 0xffe -> 0x002), count++, $49e dance ($1526 ror
  with the 0x7f -> 0xf7 guard leg, poke deferred), 36/37 steps.

## Harness notes

- Stepper starts at 0x52e (past the $0405 hardware read, S3c
  pattern) with d0 = byte; restore pops fresh-stack zeros
  (post regs zeroed, a7 0x7f00 -> 0x7f34); the 0xff leg stops
  at 0x5ec pre-restore and keeps staged regs (split
  assertions).
- One documented exclusion: the jsr $49e return-address spill
  [0x7efc,0x7f00) (S-only; narrower than S4's window).
- Event pin: exactly 1 bus event ($400001 poke, address +
  write asserted) on ring legs, 0 elsewhere -- the poke is
  logged stateless on silicon and skipped in C (cf04
  precedent: body applied, poke deferred).

## 8/10 status

Bridge pieces native: filer (v0746), drainer (8f), L3-file
(here), dispatch/drain/render (S2-S4). Missing: any live
trigger (filer callers need staged g13/fighter states;
drainer has no callers; UART bytes never observed live).
Next: compose the synthetic chain test end to end, then close
8/10 on control-complete (live packets stay the documented
unknown).
",
