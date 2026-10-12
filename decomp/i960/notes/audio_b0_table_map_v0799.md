# v0799 - B0-table target map + orphan classification (slice 2)

## Table (0x13de, 128x `jmp $disp(pc)`, e+2 rule)

- Default `0x13dc` x110 (bare rts).
- 18 singles: 0x15de (1), 0x164c (2), 0x16ba (7), 0x174c (10),
  0x181c (16), 0x18a8 (26), 0x18b6 (32), 0x1912 (41), 0x194a (42),
  0x1986 (43), 0x19c2 (44), 0x1a48 (45), 0x1a76 (46), 0x1a9a (47),
  0x1ac0 (58 = 0x3a, the live sample-upload, v0795), 0x1b4c (64),
  0x1c0a (80 = 0x50, sub-dispatcher), 0x1de0 (123 = 0x7b, loop D,
  also jsr'd once from C0 at 0x1e6e).
- Matches the v0794 19-target count exactly (18 + default).

## Orphan dispositions (all resolved, none live)

- 0x1958: body of the idx-42 handler (0x3000-table scan head at
  0x194a, 21 iters). Dead entry.
- 0x1986: head of the idx-43 handler (release-tagged voice scan).
  Dead entry.
- 0x19c2: head of the idx-44 handler (SCSP-slot +0x0d save/restore
  to 0x3250, then the wave-blob upload at 0x19de). Dead entry; the
  "second wave uploader" is this handler's tail, not a subroutine
  (no jsr).
- 0x19e2: idx-44 body (upload gate). Dead entry.
- 0x1c0a: head of the idx-80 handler (d0 == 0x7d/0x7e/0x7f
  sub-dispatcher; 0x7e arm enters loop C at 0x1d60 with a1
  pre-set). Dead entry.
- 0x1d40/0x1d5e: idx-80 body (clear+$152e=0 sub + V2-scan). Dead.
- 0x1de0: idx-123 head AND C0 subroutine. Dual entry, both mapped.

## Not table-reachable: the 0xc30 voice-setup block

0xc30-0xcb6 (+0xd98 twin) sits in the 90/80-body region between
the primer SCSP writes (0xb46) and the e0 handler (0x12b6):
`ori.l #$10300000,d6` + `move.l d6,$4000(a5)` (SCSP+0x4000
window) with ram[0x600a]/ram[0x2000+d4+1]/ram[0x601a]/ram[0x601c]
voice fill. No jsr/bra/table entry found; entry is intra-body
fallthrough from the 90/80 primer chain (post-refusal — all C
refuse points precede it). Precise join mapping goes to slice 4;
SCSP+0x4000-window semantics to slice 3. The surrounding
0xc46-0xc68 code/data mix (`$00a0` now an ORI.B imm byte,
`$463c`/`$2c0a`/table words) resolves with entry-mapped reading
there, not here.
