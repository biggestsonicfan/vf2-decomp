# v0789 - audio engine liveness map (status note; Phase 0.1 committed)

`src/` change in this slice: ONLY the Phase 0.1 dead-code
removal (commit `a699806c`, 132-/0+, sound/scsp/m68k tests
green). Everything below is measured status guiding Phase 1.

## Proven: bases, ISR, driver, sequence class table

- Boot (`0x100`): `lea $00100000,a5` (0x136) and `lea $1000,a6`
  (0x13c), raw-verified. **a5 = 0x100000 = SCSP base** (Phase
  0.3 ANSWER — the C model's `vf2_scsp_*` abstraction for the
  a5 window points the right direction; widening the modeled
  window is still open). a6 = 0x1000, as the header comment
  claims. Init chain `jsr 0x222..0x422`, then `jmp 0x2242`.
- Command-receive ISR at **0x526** (all exception vectors
  funnel through the 0x4c4-0x4fa trampolines → 0x4fe common
  code → dispatch on vector id; the comms path reads the
  incoming byte at `$0405(a5)`, stages at 0x1500(a6)+,
  enqueues a 4-byte packet at write cursor 0x1504(a6) masked
  0xfff, `addq.b #1,$1506(a6)` count, `jsr 0x49e` (ACK poke),
  `rte`. Reset path at 0x5ec clears `$0401(a5)`, reloads
  stack, `bra 0x102`.
- Per-frame driver at **0x1f16** (called from 0x1eb2/0x1eee):
  SCSP pokes, count check at 0x1506(a6), then stream
  maintenance — `jsr 0x21ba` (timer expiry → emits packets
  incl. `move.w #$be10` at the write cursor) plus 8×
  `jsr 0x1f7c` over descriptors 0x2000..0x20e0(a6).
- Stream class dispatch at 0x1f9c-0x1fba (`(a3)+` fetch,
  latch, `& 0xf0 / lsr #2`, `jmp (a0)` into table **0x1fbc**,
  16 entries). Dumped targets: 0x80/0x90/0xa0/0xe0 →
  **0x2080** (default), 0xb0 → **0x1ffc** (B0-sequence
  applier), 0xc0/0xd0 → **0x20ac**, 0xf0 → **0x20da**,
  one odd slot → 0x2052. So D0-sequence and F0-sequence HAVE
  dedicated ROM handlers; 80/90/A0/E0-sequence all share the
  0x2080 default.

## Proven: the dispatch zone has no static entrance

Exhaustive scans over the whole 0x80000-byte sound program
(absolute jsr, **`jsr (d16,PC)` = 0x4EBA — the form this ROM
actually uses; earlier v0788 scans only tried 0x4EB9/0x4EF8
and were deaf**, bsr.b/bsr.w, bra/bcc 8/16-bit, long-word
pointer refs, register-indirect jsr/jmp in 0x1000-0x2600):
NO entrance to 0x13ca (B0-table head), 0x1330/0x1356
(per-descriptor applier), 0x5fe (voice maintenance), any
B0/E0/C0 head, or any ring-drain reader of 0x1506/0x1508.
The table-indirect `jmp (a1)` at 0x13da and `jmp (a0)` at
0x1fba are the only computed transfers in the zone, and no
table feeds 0x13ca. The statically-live engine is: boot,
ISR enqueue, driver → streams → sequence appliers, sample
upload (~0x2242), plus the tiny 0x49e ACK.

## Reframed Phase 1 (this overturns the v0788 order)

The v0788 plan said "recover B0-table entries 0x12/0x2d/
0x2e/0x2f first". That is WITHDRAWN as first work: the
B0-table path may be dead code in the shipped ROM (or
RAM-vector entered — none found). First work is instead:

1. Recover the SEQUENCE appliers 0x1ffc (B0-seq) / 0x20ac
   (C0/D0-seq) / 0x20da (F0-seq) / 0x2080 (default) /
   0x2052 — these are on the statically-live path.
2. Resolve the ring-drain existence question: NOTHING
   decrements count 0x1506 or reads cursor 0x1508 by any
   static pattern (both (d16,An) and corrected (d8,An,Xn)
   scans empty). Either the drain hides behind a RAM vector
   (watch for writes to RAM jump slots, e.g. the odd
   `addi.w #$4efa,…` patch-shaped instruction inside the
   table region) or ISR-enqueued packets are consumed by a
   path not yet identified — the count==0xff stream-stop at
   0x1f26-0x1f2e is the behavioral edge to probe.
3. Only then revisit the B0 table (0x13ca): live via
   RAM vector, or dead.

## Decoder-fidelity flag (not a conclusion)

`vf2m68k` prints `lea $1500(a6),a0` for raw bytes
`41 EE 15 00` — but 0xEE is mode 101 `(d8,An,Xn)`, so the
true semantics may be brief-extension `(0x15,a6,D0)` rather
than `(d16=0x1500,a6)`. Any future work depending on indexed
addressing MUST verify raw bytes first; the disassembly
print alone is unreliable there. (Does not affect findings
above — bases/ISR/driver/table entries use modes verified
against raw or unambiguous prints.)

## File map

- `src/hardware/sound_board.c`: ONLY the `#if 0` removal
  (committed `a699806c`).
- Evidence: `vf2m68k disasm` 0x100/0x1ac/0x4c0/0x4fe/
  0x526/0x58a/0x5fe/0x1330/0x1f04/0x1f16/0x1f5e/0x1f7c/
  0x1fbc-table/0x21ba/0x2242 + raw-byte checks + exhaustive
  whole-ROM reference scans (all empty where stated).
