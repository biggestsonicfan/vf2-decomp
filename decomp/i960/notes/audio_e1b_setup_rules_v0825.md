# E1b-dig: setup-arm d2/d3 derivation rules (9/10 arc)

Status: all six engine inputs derive from board state on the
S4 shape; the E1b C recovery is unblocked. Probe pins kept
green. Full suite in the commit validation.

## Common tail 0xc1e (the S4 packet takes it throughout)

- d3 = ((BE16(voice+6)) & 0x7ff) * 20. voice+6 is stored at
  0xaa0 from $1516, so d3 = (($1516 & 0x7ff) * 20) with no
  voice read needed. Measured: $1516 = 0x0260 -> d3 = 0x2f80.
- d2 (rate word) via jsr 0x1014 with d6.b = $1512,
  d5.b = $1513 (HIGH-arm 0xb16/0xb1a; uppers irrelevant --
  every 0x1014 op is .w/.b): d0 = (D&0xf0)>>2,
  a3 = BE32(audio[0xc726+d0]) + ((D&0xf)<<5),
  d2 = BE16(audio[a3-0x600000]). Measured: $1512 = 0xe0 ->
  d0 = 0x38 -> a3 = 0x60d066 (== engine-entry a3, so entry-a3
  is 0x1014 residue, unused downstream) -> d2 = 0x1004.
- d5/a0 table pair at 0x5000+d3.w ({sample ptr, value}):
  d5 = BE32(RAM[..+4]) = 0x4a62, a0 = BE32(RAM[..]) =
  0x00f00000 (needs the 8MB sample window to step; a 3MB
  window faults the LOAD instead -- harness caveat).
- d6 = 0x10300000 | BE32(voice+0x0c) = 0x10310000.
- d4 = BE16(voice+0) (slot, via 0xa92).
- Head gates read the same pair: bit31 of +0x00 -> 0xed8,
  +0x0c word != 0 -> 0xd92 (both zero here).
- d6/entry, d5/entry(d5.b=$150e&0xf? no -- d5 comes from
  the table; entry values unused), a3-entry: residue.

## Map corrections along the way

- The S4 packet NEVER routes through 0x9ba (blow-through
  pinned); 0xa8c arrives at 277 steps with d3 = desc index
  (0x0a), a0 = descriptor ptr (0x40a0).
- 0x600c48-family tables live in the AUDIO ROM under the
  0x600000 mirror: 0x60c4a6 (rate words: 00 00 00 03 00 07
  ...), 0x60c726 (sample pointers 0x60d366 ...). The d2
  pointer (0x60d066) resolves through the AUDIO mirror in
  the stepper (silicon-map fidelity noted, not resolved).
- 32-byte rate table at 0xc46 (PC-relative data the linear
  disassembler prints as garbage words -- navigation hazard).
- Engine-entry a3 (0x60d066) decoded: 0x1014 output, dead
  (lea overwrites before any read).

## E1b recovery shape (next slice, not this one)

New `sound_board_run_engine_key(slot-inputs...)` called from
the 90-arm's setup_author tail: apply the 11-step slot
program (SCSP long + stream +0x0a/+0x01/+0x1a/+0x1c), park
at 0xcb6 unstepped, count 383 retired, UNSUPPORTED. Heads
(0xed8/0xd92) refuse. Differential: status pair, ip 0xcb6,
full RAM+SCSP equality, 383 vs S-384 (unstepped fault).
