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
   snap). Results: 30 frames across 3 contexts (sixth-fresh
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
