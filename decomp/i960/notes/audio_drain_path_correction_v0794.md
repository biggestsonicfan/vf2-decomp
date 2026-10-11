# Drain-path correction - master evidence (v0794 series)

## 1. Class switch enters at head+2 (raw-proven)

Drain 0x7a6 loads d0 = command (andi.l #$ff, upper clean), sets
$150e, scans 20 descs at $3000(a6) (match desc[1..2].w == channel),
jsr 0x81a per match with a3 = ring+cursor+1. Switch reloads
d0 = $150e (command) and dispatches (raw 16-bit disps, decoder
branch math source-verified):

- 0x90 -> 0x85a | 0x80 -> 0x10d2 | 0xe0 -> 0x12b6 | 0xb0 -> 0x13cc
- 0xc0 -> 0x1e5c | 0xa0 -> 0x1e74 (jmp-pcrel) | else -> 0x858 primer

So listed classes SKIP the 2-byte fetch primer with d0 = command;
only default classes run the 90 primer (d2 = payload). Consequence:
in every listed body, d2 = COMMAND and the "second byte" role
(payload for 90/80/e0/B0-value/c0, byte2 for A0-value) comes from
the (a3)/(a3)+ peek at head+2. v0791/v0793's payload-story (d2 =
payload-fetch, d0 = byte2-peek) describes the primer configuration,
which runs ONLY for default-class-90 and A0. e0's C formula
(command|payload<<7) was already switch-correct.

## 2. PC-relative rule: jsr/jmp-pcrel are e+2-based in this ROM

Decoder uses address+2+disp for jsr/jmp-pcrel (decoder.c:928) and
address+length+disp for Bcc (decoder.c:243). Raw bytes confirm e+2
for pcrel-EA: B0 table entry0 ext FFFC -> 0x13dc (rts, not 0x13de);
entry1 ext 01FA -> 0x15de (d6-save start, not 0x15e0); B0-head lea
$06(pc) lands exactly on entries (0x13de+4i). Bcc stays e+4
(verified: bne 0x12e0/0x130a land sensibly; bcc 0x13ca benign,
see 4). Earlier "+2 print bug" theory was wrong; prints are right.

## 3. B0 table: 19 targets, 1 reachable via drain

128x4 jmp-pcrel at 0x13de. Default 0x13dc (110). Live (18): 1/2/7/
0x0a/0x10/0x1a/0x20/0x29-0x2f/0x3a/0x40/0x50/0x7b -> 0x15de/0x164c/
0x16ba/0x174c/0x181c/0x18a8/0x18b6/0x1912/0x194a/0x1986/0x19c2/
0x1a48/0x1a76/0x1a9a/0x1ac0/0x1b4c/0x1c0a/0x1de0. B0-class commands
give index = command&0x7f in 0x30-0x3f: only 0x3a live (-> 0x1ac0
sample-upload). The other 17 live entries are unreachable (primer
dead, e0>=9 benign, no xrefs). Xrefs: 0x11d0 <- 0x11c0 (80-voice,
live); 0x1356 <- 0x1346 (e0-voice, live); 0x1de0 <- 0x1e6e
(c0/a0-body, live); 0x1a76 <- 0x1a56 (dead-transitive). No xrefs to
0x1912/0x194a/0x1986/0x19c2/0x1a48/0x1a9a/0x1ac0/0x1b4c/0x1c0a/
0x15de/0x164c/0x16ba/0x174c/0x181c/0x18a8/0x18b6 (table-only).

## 4. Dead inventory (cause each)

- Stale-branch (lands mid-instruction, never sprung in practice):
  90-low bcs 0x880 -> 0xa40 (mid-lea); 80-low bcs 0x10ea -> 0x1134
  (mid-lea); alloc-low bcs 0xaac -> 0xb78 (mid-move.b); alloc-exit
  bra 0xb72 -> 0xc20 (mid-move.w, unconditional: entry dead, see
  below); fresh-exit bra 0xaec -> 0xafc (mid-move.b, entry dead);
  compact-filler bra 0xf4c -> 0xfae (F-line, unreachable: always
  skipped by 0xf3c-bra).
- Unreachable entry: B0 primers (except benign), 80/e0/c0 primers,
  17 B0 handlers above, 0x1014-sub (only caller path-A, dead
  entry), sample-tail 0xc1e+ (path-B dead + path-A dies first),
  0x1062-sub (only caller path-B), 0xfb0-part2 (no referrers),
  0x1a76 (via dead 0x1a48).
- Benign: e0 channel>=9 -> B0 primer -> index 0 (all 16 0x602b26
  tables' byte0 = 0x00, measured) -> default rts. C keeps early
  return with this evidence.
- Refuse (conditional trap, data could spring): 90-low and 80-low
  selector<0xf0; A0 flags-bit7 (mid-lea at 0x1ed6); F0-else
  (phase, see 6; 0xFF ORI-death); 90-mode (phase, see 5);
  repurpose-scan hit ([9] != 0 would chain into alloc-death);
  reuse-scan hit (pristine would chain into alloc-death).
- Defensive-unreachable: c0 bcs (command<0x70 never routed to c0).

## 5. d3.mid == 0 PROVEN (no phase, no field)

Boot clr.l zeroes d-regs. The only full-width d3 writer on the
loop/drain/heads is main-loop 0x23d2 (move.l $0016(a0),d3), which
loads record bytes B16..B19. Records (0x2000-file) are byte-stored:
+0x16 = (8+i*8)&0xff (single move.b; wraps to 0x00 at record 31),
+0x17/+0x18/+0x19 never written anywhere (audited loop stores,
separate driver file, ISR preserves, heads/alloc/voice touch
different files). So d3 = B16 (<= 248), mid = 0, every frame from
frame 2 on; frame 1 runs no heads (count boot-zero). The 90
mode-table long index is therefore fully deterministic:
payload/byte2 + ((X+desc[4])>>8)*65536, mirrored exactly (even the
out-of-table case reads the same image byte). No d3mid field.

## 6. F0-else phase + FF death

F0-block else (classes F1-F6/F8-FE): ORI.B #0,d3 then beq on
d3.low (record+0x16-low: 8+i*8, zero only on record-31 frames) ->
phase-dependent branch, both arms deterministic -> refuse whole
else path. FF class -> ORI.B cascade death -> refuse. F7 (skip),
F0 (tail) clean.

## 7. Voice[0] = slot*0x20, slot = word>>5

Init 0x3a8 loads voice[i][0..1] = i*0x20 from ROM 0x6006e6 table
(measured 32 words) and voice[i][0x0c..] longs. No post-init
writer touches [0..1] (audited alloc/helpers/clears/tails). C's
`word & 31` (= 0 for all ROM voices) is wrong; use word>>5 with
<32 bound (fail-closed). ROM SCSP forms: absolute $4000/$400b/
$400d/$4004/$400c/$400e/$4010/$4012/$4016 + slot-indexed
$13/$15/$08(a5,d4.w) with FULL d4.w. C's per-slot-everything is
wrong except the (a5,d4.w) forms.

## 8. 90-path live shape: prefix only, then refuse (revised -
   high arm proven unreachable via drain-match)

d2 = command (0x90-class) or payload (default primer); second byte
= payload or ring byte 2. Base $150f = (d2+desc[4])&0xff (d3mid =
0). second == 0 -> 0x10e4-mid trap (refuse). Mode $1510 =
ROM[0x60c6a6 + (((d2+desc[4])>>8)<<16) + second] (exact long index).
Then refuse unconditionally:

- The only matchable shape (drain matches desc[1..2].w == channel
  <= 15, so desc[1] = 0 and desc[2] = channel <= 15) takes the low
  arm, which is a stale branch into mid-lea. Trap on HW.
- The high arm needs desc[2] >= 0xf0, which cannot match the drain
  (word >= 0x00f0 or >= 0x0100, never <= 15). Unreachable code;
  row/empty/$1511/$1512/$1516 formulas kept in this note only.
- Everything past (0x8d2 mid-instruction trap on desc[13] == 0,
  which nothing live ever sets; scan/compact/kill/alloc with stale
  exits at 0xb72/0xafc/0xf4c) is post-trap: no 90 packet completes
  on ROM. C mirrors the deterministic prefix ($150f/$1510) and
  refuses.

REMOVED as fiction/dead: free-scan alloc, fresh/reuse fills,
per-slot SCSP alloc writes, 90 fast/slow voice-update, 0x5000
gate, 4-byte voice_value compare, 0x1014-sub, sample-tail,
b6/b8-kill (dead entry), compact (post-trap), $1515 write
(post-death). 80's own loops keep their (corrected) slow path
(v0795a); 90 never calls the helpers.

## 9. Other bodies

- 80: base = (command&0x7f)+desc[4..5]-word, then refuse: same
  match-impossibility as 90 (high needs desc[2] >= 0xf0, low is a
  stale branch into mid-lea). The row/$1515/voice-update code below
  is unreachable via drain-match (kept in this note only); v0795a
  reduces dispatch_80 to base + refuse.
- e0: formula kept (command|payload<<7); ADD stale-word on neg
  path (result |= ((payload&0x7f)>>1)<<8, payload-derived,
  deterministic); index post-neg (already); tail 0x1356
  (SCSP+0x10 + sound_ram[0x601a] = 0x2000+0x401a, LIVE ram write -
  v0793's ROM-space verdict was wrong); voice match word+0x7f.
- B0: entry = command&0x7f; only 0x3a (sample-upload 0x1ac0,
  deterministic); rest default-rts-OK. REMOVE 12 dead handlers.
- c0: command source (bset#7 if >= 0x70; arm unreachable below);
  body = 0x1de0 (channel zeros + absolute SCSP + ram[0x2000] = 0 +
  word-match + unconditional --). C's key_off-based clear is wrong.
- A0: row = payload&0x7f, value = byte2 (v0792 inverted); primary
  (byte2<=max) = pure no-op (writes hit ROM); stale (byte2>max) =
  slot0-init (clear+a2+=3+ptr+bits, mirror); flags-bit7 = refuse;
  scan path (0x1ed4) for ... (same slot-init shape, slots 1-6).
- 0x1de0/0x11d0/0x1356 tails as dumped (absolute + (a5,d4.w)).

## 10. Init mirror (0x28e/0x320/0x336/0x3a8/0x3cc/0x402/0x422)

SCSP regs ($0400-block volumes, $0418/$041a/$0422 timers, $0401
seq, $17(a5) vol-table from ROM 0x605368-table); RAM clear
0x1000+0x3F00 + $1526 = 0xef; desc-file 2x0xa0 from ROM ptrs at
0x603066/0x6030a6 + $151f = 0xff + desc[10] precompute + $151a =
0x2800; voice-file (above); stream-file 32 records
(+0x0e = 0x10000+i*0x2000, +0x12 = i*2, +0x16 = 8+i*8); sample
upload (0x422, same shape as B0-0x3a) + $1526-dance. C's
memset-zero init diverges on all of these.

## 11. Sequence/driver (v0796)

Worker 0x1f7c (countdown/stream-ptr/stale-byte/class appliers/
tail-countdown-word); appliers 0x1ffc (B0/80: raw,byte + 0x10-path
with $000c/$000e latch), 0x2052 (A0: bclr-store + 127),
0x2080 (default: raw,raw,consume-test), 0x20ac (C0/D0: bclr-store
or 0), 0x20da (F0: F7/F0/FF/else); tail 0x211e (cursor/count/sr/
chain/countdown); driver 0x1f16 (count-0xff gate, SCSP timers,
8 slots, rte); prescaler 0x21ba (BE10-heartbeat packets +
countdown + mainloop-abort). C's stream layer (F-escapes, timers,
chains, 4-byte packets) matches none of this - rewrite.
