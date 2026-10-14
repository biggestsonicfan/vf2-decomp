# S3c: LOW authoring + ch-0xf path via synthetic entry (v0825)

Status: 0xa8c LOW branch and the ch-0xf 0xaf0 seeding path
recovered in C behind the new public vf2_sound_board_setup_author
entry (indices, bounds-checked); both proven by synthetic-entry
differentials (garbage-seeded registers, C==S incl. live d2 low
word). Refuse at 0xc1e, packets... no packet at this boundary --
effects + UNSUPPORTED at the door. Sound gate green.

## Why synthetic entry (reachability, proven)

- LOW branch: desc[2] < 0xf0 routes to 0xa3e pre-Loop-B, and
  nothing mutates desc[2] between the 0x878 split and the 0xaac
  split. No drain packet can arrive at 0xa8c with a LOW
  descriptor. Proof by chain construction.
- ch-0xf 0xaf0 path: ch-f packets take Loop-B's 0x9a2 branch and
  never arm, while authoring runs only on the just-armed voice
  (whose +4 = this packet's non-f channel). Same class of proof.
- The C keeps refusing nothing new: dispatch_next behavior is
  unchanged (HIGH/ch!=0xf as before); the LOW/ch-f bodies were
  never reachable through it. The public entry is the honest
  vehicle -- no fake drain fixtures that silicon would route
  elsewhere.

## LOW branch (0xb76-0xc1a)

+3 = $1515; +8 = $1510; +4 = ch; +9 = $151e ($151e++);
+10 = $1518. No +2 bsets, no +5/$152e traffic (voice+5
preserved -- pinned 0x77). Volume math shared with HIGH
(factored helper). 0x1062 picker: seed = word(RAM 0x4200+2ch);
slide = sign16(seed + sign16(byte(voice10+desc5))); picker =
word(be32(0x602676+(voice3&0xf)*4)+2*slide); store =
(((voice3&0xf0)<<7)+picker)&0x7bff; d2 =
word(be32(0x60c726+((voice3&0xf0)>>2))+wrap((voice3&0xf)<<5)+
2*slide). lsl.l residue / dead (a3)+ / zero-upper d6 all
audited in-code. Post: sample-slot words, d5 = desc8w |
((ram4220+ch<<5)&e0) | (ram4240+ch&7) (TWO channel rows --
HIGH uses one), 0x15 = desc[12] (not $150d), 0x16 =
($1511&0x1f)|0x60 (vs HIGH's 0x70).

## Legs (84 / 75 steps, garbage d/a regs, C==S)

- low: full voice record (40/41/03/77/12/ff/20/07), $151e
  0x20->0x21, $152e preserved, volume corner 0xff*0xff -> 0x00
  absolute, d2 low == lookup_word (upper = garbage seed on
  silicon -- move.w preserves it; the C models the defined low
  word only, documented).
- high-f: +2 = 0x50 (no 0x08 bset), +9 = $151e, +5 preserved,
  d2 low == lookup_word.

## 0xc1e tail survey (S4 approach)

0xc1e-0xc44 (10 insns): d3 = voice+6 & 0x7ff * 20 ($5000
sample-table stride); SCSP[slot+8] = long(entry+0x10);
voice+0xb = table[PC+10+(entry+0x13 & 0x1f)]; bra 0xc68. The
0xc68 engine is movem block-copy loops (sample upload to SCSP?)
with an rts exit at 0xf10 via the 0xed8 sample-key path, then
the 0xf12 compactor. S4 needs the sample-ROM model (banks
absent locally -- control-only per plan) + engine recovery;
the tail authors the last voice bytes S4 starts from.

## S3 close-out

7/10 criteria 2-3 (Loop-B + setup arms) are done except the
0xc1e tail + engine (S4) and the 90 LOW path + B0/C0 heads
(backlog: LOW path needs its own row-table work; B0/C0 the
primer differentials). S4 next: chain dispatch->Loop-B->setup
HIGH on real ROM with zero-filled sample windows, render keyed
voices (control-complete WAV).
",
