# v0821 - V1: voice-ISR re-recovered, triangle unpinned

Status: `maintain_voices` rewritten to the true 32-voice scan;
`vf2_m68k_triangle_differential` green WITHOUT `WILL_FAIL`
(5/5 L1 + e0 + upload). Coordination note: the voice ISR is
kazuyette's slice per v0816/R5 -- this recovery takes their
`audio_pcrel_base_reaudit.md` Slice-2 disassembly as the spec
(fresh 64-instruction dump at 0x5fe verified identical under the
fixed decoder) and corrects exactly one conclusion in it, with
byte evidence. No oracle change (`src/m68k` untouched).

## What was wrong

The old C modeled a +4 artefact: iters 2-32 entering at 0x61c
(mid-`btst`, "ori.b bodges"), a0 frozen on voice 0, voices 1-31
frozen, expiry refusing at the rebalance "trap". Under the +2
base the ISR is ordinary coherent code: outer `dbf d7,$61a`
reaches the `btst` start every iteration, `adda.l #$10,a0`
advances, all 32 voices serviced, both rebalances live, `rte`
always reached (stepper: 138/140/285/346 steps, a7 balanced,
zero events on every leg).

## The one correction to their spec (byte-verified)

Their note says the rebalance compares `d2.w` (move.b-loaded,
upper = ISR-entry residue) against the word at voice+9, and
proves with MAME taps that d2 takes hundreds of entry values --
concluding a faithful C must take d2 as input. The opcodes say
otherwise: 0x66e `B412` and 0x6c0 `B42A` both have opmode 000 =
BYTE (this is the corrected v0820 CMP map, not the rotated
print). The whole chain is byte-closed -- `move.b` load,
`cmpi.b`/`cmp.b` compares, `subq.b` decrements -- so the residue
never participates. Their MAME data stands; the "d2 as input"
conclusion is void. The C is deterministic over RAM.

## The recovery (sound_board.c)

Per voice: skip unless bit0 of +2; `voice[0xb]--`, skip unless
zero; bit3 picks release (0x684) vs normal (0x636) arm. Both
arms: key-off (`sound_board_key_off` reused -- both ROM arms are
the identical `move.w`/`move.b`/`move.b` triple), clear +2,
snapshot d2 (normal: +9, release: +5), clear +9 (release also
+5), decrement `$151e`/`$152e`, then the rebalance over all 32
(including self -- silicon does too; the expired record always
skips via its cleared bytes):
- normal: +9 bytes `!= 0xff` with `mem > d2` (unsigned `bcc`
  skip) decrement;
- release: +5 bytes of records whose +9 `== 0xff` with
  `mem > d2` decrement.
`voices_aged` counts rebalance decrements (struct field existed,
old code never filled it). Key-off OOB still fails closed.

## Legs (test_triangle.c)

- s1-clean (140 steps): voice0 40->39, voice1 7->6, no expiry.
- s2-wrap (140): timer 0->0xff, `bne` skips expiry.
- s3-skip (138): bit0-clear voice0 untouched (0x28).
- s4-expire (346): timer 1->0 normal arm, slot 0x20
  (SCSP[0x2b]=0x1f), `$151e`--, d2=5: voice1 +9=3 skips,
  voice2 +9=7->6.
- s5-release (285): timer 1->0 release arm, slot 0x40
  (SCSP[0x4b]=0x1f), `$152e`--, d2=3: voice2 (+9=0xff) +5=7->6,
  voice3 (+9=0xff) +5=2 skips.
- e0 (204): restructured head-entry (see below), arbitrator
  agrees three ways (C=S=shaped=0x1b6a).
- upload: unchanged silicon-equivalent refuse (C=8 S=8).

Report asserts (scanned==32, expired/normal/release) pin arm
selection on the C side; RAM+SCSP equality with the stepper is
the proof. `smash_stack`/bodge asserts retired (no bodge; the
movem save-area asserts stay -- still exact).

## T1 restructure: tail-only -> head-entry

The old T1 poked shared `$1524` = 0x0201 on the stepper side.
That value was reachable ONLY under the retired S7b stale rule
(payload upper riding in); the true head (andi.l LONG, uppers
zeroed) yields `+-selected` (a byte), so 0x0201 is unreachable
garbage. T1 now enters the stepper at the head (0x12b4, a3
stream mirrors ring, `$250e` filed as the drain would) and runs
to the tail exit 0x1354; shared is an agreement (proven by the
S7b cdispatch differential), and the arbitrator reads it back
from the C RAM instead of hardcoding it. Two harness
asymmetries documented in-test, not asserted: the 0x7300 stream
staging (staged both sides) and the jsr/rts stack window
[0x7ECC,0x7F00) (stale return address; balance proven by a7).

## v0792 unit test re-pinned

The `maintain_voices` block now pins the true contract on the C
alone (same code path the triangle proves): OK status,
32/2/1/1 scanned/expired/normal/release, aged==2, both counters
decrement once, both key-offs commit (slot-0x0004 sentinels
FALL), voice2 +5 = 7 witness.

## Remainder

- Loop-B stays `WILL_FAIL` (separate slice): F1/F3 pin +4
  mid-instruction landings; the 0xcb6 `movem.l d0-d7,(a2)` gap
  needs measuring before re-pinning.
- `init-on-real-ROM` failure still open (noted in S7b, blocks
  nothing).
- Audibility chain: V2 sample-upload leg, V3 key-on + render +
  WAV probe. The per-frame `game.c` voice call now returns OK
  instead of refusing on every expiry -- strictly closer to a
  running tick.
