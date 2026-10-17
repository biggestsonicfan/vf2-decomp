# 8c-hunt: sound comm port (v0825) -- receiver mapped, writer unreached

Status: HUNT, no src/ change. The sound-side receiver is fully
mapped from the ROM; the main-side writer is addressed but has
no live event yet. The bridge recipe below is the next slice.

## Main side: UART at 0x009c0000 (MAME Model 2A serial)

MAME model2.cpp: all Model 2 revisions carry an 8251-compatible
serial controller for main->sound; on 2A/2B-CRX it sits on the
32-bit primary bus at **0x009c0000** (2B-CRX: 0x9C0000 in the
315-5797; original Model 2: 0x01c80000 secondary bus). VF2 is
Model 2A, so 0x009c0000 is the writer address. Sound IRQ fires
on RxRDY/TxRDY via interrupt line 10.

Static VF2 evidence (out/maincpu.bin region, LE scan):

- 3 occurrences of bytes `00 00 9c 00`, ALL data, zero code:
  - 0x111e2: inside a debug name table (follows the string
    "debug_mode\0") -- address-annotated symbol entry.
  - 0x29bd8 / 0x50db4: identical MMIO address tables (16 B
    apart sits 0x01800000 = palette base at -16: a hardware
    table with palette @-16, serial @+0).
- Zero LE references TO those table addresses anywhere in the
  2 MB image: the writer reaches them through computed
  addressing (base + index), not absolute loads.
- Our Model2A models NO 0x009cxxxx range (map covers main ROM /
  data / geometry / copro / work / buffer / video / timers /
  tiles / palette / IO / SRAM / textures / luma / system only).

Live oracle evidence (vf2probe --memory-trace, passive):

- sixth-fresh.vf2snap + 100000 steps (gameplay): 150,024
  records, ZERO 0x009c accesses, run clean.
- sega-natural-frame.vf2snap + 100000 steps (boot): 7383
  instructions to self-branch halt, ZERO 0x009c accesses.
- sixth-fresh + PUNCH held (0x10) + 300000 steps: clean, zero.
- sixth-fresh + KICK held (0x20) + 300000 steps: clean, zero.
- indk-c.vf2snap (TEST MENU coin context) + PUNCH + 100000
  steps: clean, zero.
- FIVE windows, zero traffic. AND the three sixth-fresh runs
  (no-input / punch / kick) all park at the SAME ip=69536 with
  IDENTICAL call/return counts (10306/10305): the inputs had NO
  effect. vf2probe never injects frame interrupts, so the game
  sits in its frame-wait spin forever -- held inputs are never
  consumed, no gameplay events fire, no sound writes can occur.
  The negative is therefore "writer unreachable from a parked
  probe", not "writer absent".

## Sound side: L3 receiver ISR fully mapped (0x526)

Vector table (BE32 in the audio region): L1->0x6005fe,
L2->0x601f16, L3->0x600526, L4-L7->stub cluster at
0x6004e8-0x6004f4. (Pinned by the vectors leg in
test_cdispatch: reset/L1/L2/L3 entry words.)

- **L2 @0x1f16 = stream tick** (autonomous engine, NOT the
  receiver): SCSP pokes ($418/$422 via a5), $1506 gate, then
  0x21ba + per-stream 0x1f7c ticks over the eight $2000+
  records (stride 0x20, matching V2's stream init). The 0x1f7c
  engine (countdown-gated byte streamer into a 16-way jump
  table at 0x1fbc) is the 0x80-class playback arm -- future
  9/10 material, surveyed not recovered.
- **L3 @0x526 = UART receiver.** Head: `move.b $0405(a5),d0`
  (the $0405 hardware read V1 already noted as "waste" during
  init -- it is the UART data register, 0x100405 with the
  fixtures' a5=0x100000). Then:
  - d0 == 0xff -> RESET path @0x5ec: `sr=#$2700`,
    `$401(a5)=0`, `a7=$5000`, `bra $100` (= reset vector
    0x600100: main can reboot the sound CPU over UART).
  - d0 >= 0xf0 -> ignore arm @0x5e6 (but still files $1503).
  - else bit7 clear/set split ($1501 vs $1500 file + $1502
    count math), then BOTH converge: ring-file two bytes at
    $1500+($1504&0xfff), $1504+=4 (&0xfff), $1506++,
    `jsr $49e`, `$1503=d1`, restore, rte.
  - The ring format ($1500/$1501 bytes, $1502, $1504 cursor,
    $1506 count) is exactly what the C drain consumes --
    receiver output plugs directly into dispatch_next input.
- **$49e dance MAPPED** (closes the AGENTS.md backlog item):
  `ror.b #1,$1526; ==0x7f ? force 0xf7; store; (low nibble |
  0x10) -> $400001` (the unmodeled V1 system port). A rotating
  flag + hardware poke per received byte. 0x4c4+ is the
  exception-code table (moveq 14/13/12/11/8/7/6/2/5/4...).

## Bridge recipe (next slice, NOT this one)

1. Model an i8251 data-register write at i960 0x009c0000:
   file the byte through the L3-equivalent path (C ring file +
   $49e poke). Fail closed on status/control-register shapes
   until measured.
2. Prove end-to-end with a LIVE writer event. vf2probe cannot
   produce one (no frame-interrupt injection -- see the parked
   ip=69536 evidence above). What is needed is an ORACLE-FRAME
   DRIVER: vf2_session_step_oracle (or the hybrid frame-wait
   observer) with the Model2A memory observer attached,
   stepping real frames with varied inputs (round start is the
   prime suspect: match-start BGM; coin sound second). The
   writer IP falls out of the first observed 0x009c0000
   access; its byte + surrounding disassembly is the bridge
   fixture. Tooling slice first, bridge second.

   UPDATE (8d driver built, still unreached): the driver exists
   as tests/host/test_session_uart.c + the 5-line
   vf2_session_set_memory_observer forward in session.h/c
   (passive; NULL detaches, invalid session fails closed).
   ctest entry vf2_session_uart_probe (10 oracle frames from
   sixth-fresh, punch pattern; skips 77 without the gitignored
   snap). Later upgrades: frame-budget argv[5] (default 10,
   pattern cycles, capped 2000, stop-on-first-UART-write) and
   odd-frame alternate mask argv[4] for edge acceptors. Results: 30 frames across 3 contexts (sixth-fresh
   punch-pattern, indk-c TEST MENU punch-pattern, park-warm-coin
   COIN-held), every frame VF2_OK with changing digests, ZERO
   UART writes. The negative is load-bearing: the hook's
   positive control counts every observed access (2.6M over the
   sixth-fresh run, ~260/accesses-per-instruction through the
   public bus symbols the executor calls), and the test FAILS
   any frame with zero observations -- a dead hook cannot pose
   as a silent game. Bonus proof: failed UART writes would abort
   the frame (unmapped range), so VF2_OK frames mean genuinely
   absent, not unobserved. Next: round-start/match-boundary
   snapshots, or static hunt for the MMIO-table users.

   UPDATE (attract negative + main-loop map): 10 oracle frames
   from sega-natural-frame (boot/attract, START at frame 5):
   4.5M observed accesses, hook live, ZERO UART writes -- even
   the attract jingle does not cross the UART. 68k reset entry
   (0x100) verified by disassembly to match the C init exactly
   (0x222 vector copy, 0x28e SCSP, 0x320/0x336/0x3a8/0x3cc/
   0x402/0x422 inits, `jmp $2242`). Main loop at 0x2242: flag
   byte at (a0) gates `jmp $7a6` (DRAIN call from the main
   loop), $1c/$1a counter pair, $0409(a5) bit-7 MIDI gate
   (v0813's register), then movem stream programming. Working
   model: BGM/streams run AUTONOMOUSLY (boot-loaded $2000
   records + L2 tick, no ring involved); the $1500 ring + UART
   carry ONE-SHOTS (hit SFX, coin, announcements). That is why
   40 frames of whiffed punches, menu edits, held coin and
   attract are all silent on the UART. Next fixtures, in order:
   (1) coin-EDGE toggling (held COIN is level, the acceptor
   wants 0->1; needs a per-frame-toggle pattern in the driver);
   (2) a LANDED hit (attacker in range -- needs a setup
   snapshot, not pressure); (3) round-announce (needs a
   round-boundary snapshot). No src change this slice.

   UPDATE (coin-edge toggling, still silent): the driver takes
   an optional argv[4] odd-frame alternate mask (A,B,A,B...) and
   prints the per-frame input. Two 0/COIN toggle runs, 5 rising
   edges each, both VF2_OK with live hooks:
   park-warm-coin + indk-c TEST MENU, zero UART writes. Inputs
   demonstrably steer the game (toggle digests diverge from the
   held-COIN run exactly on the first 0-frame). Six contexts /
   60 oracle frames total, all silent. The acceptor wants a
   different pulse shape, or coin sound does not cross the UART
   in these states either. Next: LANDED hit (setup snapshot) or
   round-boundary (long oracle play from boot).

   UPDATE (SOUND TEST detour -- queue architecture found,
   recipe park-mismatch): recovery note selector17_index2_sound_test
   documents the other half of the design: the i960 holds a
   WORK-RAM sound QUEUE (PUNCH files 0x00ad1001, cursor 3->4;
   TEST files five stops, cursor ->8; lever walks BGM/SFX names
   "No.0 Advertise".."No.296 Wo13"). So the writer is really TWO
   unknowns: the queue FILER (game/test code, proven to exist)
   and the queue DRAINER (queue -> UART bytes, never observed).
   The note's deterministic recipe (0xa6c0 -> 0xa010, ~1873
   insns) did NOT replay from park-sixth-a6c0.vf2snap: three
   variants (plain, full input/phase staging per the note)
   all run an identical 36-step tail with identical call counts
   -- the park point sits PAST the recipe's input gating, or the
   pre-branch state needs more than $500700/$500704/phase-a4.
   The recipe needs a pre-branch snapshot (sixth-fresh is the
  top-level menu park candidate) or a seq-capable driver
   (walk-then-enter isn't expressible in held/alt masks). The
   queue ADDRESS itself is undocumented -- grep the note for it
   before the next attempt; without it the queue can't be
   watched directly either.

   UPDATE (table-watch: infra stone cold, HUNT ARC CLOSED): the
   driver also records READS of the three static 0x009c0000
   entries (debug @0x111e2, MMIO @0x29bd8/@0x50db4 -- ROM data
   loads travel the observed public bus symbols, so any fetch of
   the UART address from its tables prints as an R hit). Four
   more runs (sixth-fresh, sega-natural-frame, park-warm-coin
   toggle x10 frames each, plus a 500-frame sixth-fresh
   long-play with punch held AND a 1000-frame early-boot play
   through attract entry AND walk-in/stand-still contact pairs
   AND three transition states -- test-mode exit, coin-assign,
   manual-setting rearm): ~400M observed accesses total, ZERO
   hits of any kind -- no table read, no UART write, anywhere
   reachable. Companion static proof: all 53 $150x-constant
   sites disassembled -- the $1500 ring has EXACTLY ONE producer
   (L3 UART ISR) and the drain/Loop-B are pure consumers, so BGM
   provably never crosses the ring (streams + L2 tick carry it
   autonomously); L1 fully mapped as the voice-maintenance tick
   (no switch polling). The i960 sound path is never touched in
   attract / gameplay / menu / coin / transition contexts. Remaining triggers (landed hit needing
   a setup snapshot, round-boundary needing hours of blind
   oracle play at ~3k insns/frame) are staged-fixture work. The
   bridge stays unmodeled per fail-closed. 8/10 continues on its
   completable tranches (staged-packet session WAV plumbing, V3b
   voice naming); the hunt resumes when a round-boundary fixture
   exists.
3. Do NOT tick the 68k per-frame as a shortcut: the L2 stream
   engine + Loop-B are autonomous 68k programs, and silently
   stepping them in-session would hide unrecovered engine
   behavior behind apparent music. UART bridge first (it is
   the architected channel); autonomous tick is 9/10 scope.

## Correction log

- S4b bank order (numeric 70..73) is backwards vs the real
  SAMPLES region (73/72/71/70); 8b leg uses the builder.
  (Carried from the 8a/8b note -- repeated here because the
  bridge slice will touch sample windows again.)
",

## v0853 correction -- the "no live writer event" premise is void

- The 8d driver watched **0x009c0000**, a table-resident lead.
  The drainer stages the ROM-immediate **0x01c80000** (0x43860
  `lda 0x01c80000, r3`), and the pump reads that slot back.
  Zero hits there measured the wrong address, not a silent game.
- The oracle services no interrupt but the frame's (vector 12),
  so the ROM's only UART writer -- the vector-15 drain ISR -- had
  never run. It now has: `vf2_session_service_sound_drain`
  drains the live park queue byte by byte (9 bytes
  `ae 10 1f 9e 0a 7f ae 10 1f`, 10 services, 289 instructions,
  self-disarm to enable 0x21). See `audio_isr_v15_live_v0853.md`.
- The gameplay-writers hunt is NOT invalidated; only its
  negative is. The source side (a gameplay event that files)
  remains open; the pointer is `byte[0x500f00] == 1` at the
  flusher 0x43abc (measured 0 in every parked state).
