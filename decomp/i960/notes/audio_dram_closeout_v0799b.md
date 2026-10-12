# v0799b - DRAM-sample close-out audit + move-Dn-dst decoder fix (slice 3)

## Second decoder fix (same shape as v0797)

The `move Dn,(d16/d8,An)` handler merged modes 5/6 in print
(dropping the index reg) with coincidentally-correct length 4.
Fixed with the same DA-correct brief-ext print. The sibling
`move An,(d16,An)` handler is mode-5-only and clean. Vectors: ROM
bytes `1d 41 15 0d` (mode-5 guard) + `27 85 40 0a` (mode-6,
`move.l d5,$0a(a3,d4.w)`). Gate: old code + new vectors FAILs;
fixed green.

## Blast radius: e0-tail + 0xc30 block re-read

- e0 tail 0x1396 (`3b 80 40 10`): TRUE `move.w d0,$10(a5,d0.w)`
  (d0-scattered far write), not SCSP+0x10 absolute.
- e0 tail 0x13c4 (`35 82 40 1a`): TRUE `move.w d2,$1a(a2,d0.w)`
  with a2 = 0x2000 — wait, ext 0x401a is d4-indexed: TRUE
  `move.w d2,$1a(a2,d4.w)` = ram[0x201a+d4] (stream-slot timer,
  d4 = voice[0]), not ram[0x601a]. The v0795 "0x601a live RAM"
  address was the artifact; the liveness intuition (RAM, not ROM)
  survives at the corrected address.
- C fixed: ram[0x201a+d4] modeled exactly (bounds-checked);
  SCSP[d0+0x10] attempted VDP1-style (odd -> UNSUPPORTED
  address-error shape; beyond-file -> tolerated no-op; in-range ->
  lands in the file exactly as addressed, model-interpreted).
  Fixtures: large-d0 (tolerated scatter + 0x201a word + SCSP+0x10
  stays zero) and small-d0 variant (shaped 0x0020 -> SCSP[0x0030]
  lands; row base repatched for row 0). 4 plants fail exactly.
- 0xc30 block re-read with fixed prints: `move.l d5,$0a(a3,d4.w)`
  = ram[0x200a+d4], `move.w d2,$1a(a3,d4.w)` = ram[0x201a+d4],
  `move.w d2,$1c(a3,d4.w)` = ram[0x201c+d4] (a3 = 0x2000 throughout),
  `move.b #$10,$01(a3,d4.w)` unchanged. The whole voice-fill targets
  stream slots, coherent with the V1 key's ram[0x2000+d]. The
  `move.l d6,$4000(a5)` (0xc8e, opcode 0x2B86 mode-5 verified by
  hand-decode) is the one genuine far-window write: SCSP+0x4000 =
  0x10300000 | table[d5&0x1f], unmodeled-device boundary, C has no
  equivalent (post-refusal).

## KEYONB/SA sweep verdict (DRAM unwired, upheld as revised)

- No genuine (d16,a5) writes to slot offsets 0x00-0x07 anywhere:
  the only hits are btst-imm artifacts and SUB/OR reads (the
  ($80,a5) swarm = slot-4-state polling reads in dead/post-refusal
  bodies; the five ($1080,a5) + ($1082,a5) = far-window reads;
  0x12cf2 = table-data coincidence in the 0x12ce0 bit-test data run).
- Slot+0 writes: boot/V-keys byte 0x10 only (high byte untouched,
  0x0800 never set deliberately). e0-scatter can land on control
  words address-faithfully (model-interpreted, same as HW).
- SA regs (slot +2/+4): never written in any frontier path, so the
  slot file stays zero and no DRAM address is ever programmed.
- Primer/vol indexed writes ($08/$13/$15/$17, tail $01/$06): stored
  as register bytes, uninterpreted by the spec-written DSP model:
  boundary, no code.
- Revised v0796 claim: it is not that "all sample-start code is
  dead" — the 0xc30 voice-trigger (SCSP+0x4000, post-refusal) is
  live-reachable on ROM whenever a command matches. The precise
  boundary: no path programs DRAM sample addresses; the trigger
  window is unmodeled; the C refuses before it. Fail-closed stands.

## Main-loop activation tails are live-unrecovered (for slice 5 admin)

The 0x24d2/0x260c tails (SCSP slot+1/+6 writes, MIDI poke $0408,
voice[2]-bit0 sets) sit in the live main loop, not behind refusal.
The C models none of it (game.c pump = dispatch + L1 tick), so C
voices never start: coherent fail-closed, but "dead" is the wrong
word. Distinct from the dead countdown half. Noted here, filed in
slice 5.
