# v0826 - SCSP DSP program measured: VF2 2.1 boots a real reverb program

Status: evidence note + one ROM-backed test leg (no `src/` behavior change).
Answers P5's DSP TBD (v0802) and corrects v0804/v0805.

## Question

P5 (v0802) left "DSP effect / DSP program loader" as TBD. v0804 scoped its
scan to `0x400-0x43F` and titled itself "no DSP program". v0805 called
`0x0700` the "wave area". Both are superseded by this note: the `0x700-0xBFF`
window is the SCSP DSP register surface, and the boot uploads a real
effect program into it.

## Method

1. Full-window census of every `a5`-relative reference over the linear
   disassembly (`out/m68k-full.dis`, 232,503 lines) across all addressing
   classes: `(d16,a5)`, `(d8,a5,Xn)`, `lea $disp(a5)`, and
   post-increment stores after a `lea $disp(a5),aN`. Re-derived slices:
   `out/scan_master.py` (0x400-0x4FF, 28 hits) and
   `out/scan_ring_dsp.py` (0x500-0xBFF, 37 hits).
2. Code/data adjudication: the code/data boundary is `0x2676`; every
   reference past it with an incoherent window (`btst`-sleds, repeated
   `or.b $0NNN(a5),dN`) is a phantom (all 37 hits in `0x500-0xBFF` are
   such phantoms except the two `lea $0700(a5),a0` sites).
3. MAME YMF292 reference (`scsp.cpp` / `scspdsp.cpp`) as the register
   window and step-semantics guide - suggestions only, each pinned below
   by a ROM site or a measured decode.
4. ROM table parse (`out/test_constants.py`): DSP program table
   `0x60493e`, effect-return table `0x605368`, per-bank instruction decode
   (`out/dsp_check_out.txt`).
5. The existing C boot mirror (`src/hardware/sound_board.c`) as the
   software-visible contract; the new test leg pins it against the ROM.

## Register window map (MAME decode, `scsp.cpp:1040-1140`)

| range | contents |
|---|---|
| `0x000-0x3FF` | 32 slots x `0x20` (byte 0x15 = ISEL/IMXL, 0x16 = DISDL/DIPAN, 0x17 = EFSDL/EFPAN) |
| `0x400-0x5FF` | master block (`0x402` = RBL/RBP, `0x405` = MIBUF, `0x408` = MSLC latch, `0x418/0x41a/0x41c` = timers A/B/C, `0x424-0x428` = SCILV0-2) |
| `0x600-0x6FF` | RINGBUF, 64 words (FM path; FM absent, v0807) |
| `0x700-0x77F` | DSP COEF, 64 words |
| `0x780-0x7BF` | DSP MADRS, 32 words |
| `0x7C0-0x7FF` | MADRS mirror (MAME aliases it twice) |
| `0x800-0xBFF` | DSP MPRO, 512 words = 128 steps x 4 |
| `0xC00+` | MEMS/MIXS/EFREG/EXTS readback |

Writing the MPRO word at `0xBF0` calls `m_DSP.Start()`
(`scsp.cpp:1074`); `Start()` scans steps 127 down to the last nonzero step
and sets `LastStep = i + 1` (`scspdsp.cpp:294-306`).

## The boot runs the program (live, ordered)

```
0x1b0: jsr $0000028e     ; program upload
0x1c4: jsr $00000402     ; effect-return arming
0x28e: move.b #$0,$0401(a5)
0x294: lea $0700(a5),a0          ; window base
0x298: lea $0060493e,a1          ; program table
0x29e: move.w (a1)+,d7           ; count word (ignored here)
0x2a0: move.l (a1),a2            ; pointer 0 -> 0x604948
0x2a2: move.w #$4ff,d7           ; 0x500 bytes
0x2a6: move.b (a2)+,(a0)+        ; byte burst
0x2a8: dbf d7,$000002a6
0x2ac: move.w #$138,$0402(a5)    ; RBL=2 (32K words), RBP=0x38
```

The burst's write to `0xBF0` (window byte `0x4F0` = MPRO step 126's first
word) triggers `Start()`. Both programs keep steps 126-127 zero, so the
mid-burst scan computes the same `LastStep` as a scan after the full copy
(82 for blob0, 44 for blob1) - the start trigger is not a race.

`0x402 = 0x138` decodes as `RBL = (v >> 7) & 3 = 2` -> 32K-word ring,
`RBP = v & 0x3f = 0x38` -> the DSP delay ring lives at word addresses
`0x38000-0x3FFFF` (bytes `0x70000-0x7FFFF`).

`0x402` (called from `0x1c4`) arms the effect return: count word at
`0x605368` (ignored), blob0 pointer, then 16 bytes -> slot byte `0x17` of
slots 0-15 (`move.b (a0)+,$17(a5,d4.w)`, `d4 += 0x20`).

## The two programs

Table `0x60493e`: count word `1` (max index), pointers `0x604948`
(blob0), `0x604e58` (blob1). Each blob is `0x500` bytes:
COEF `0x000-0x07F`, MADRS `0x080-0x0BF`, zero pad `0x0C0-0x0FF`
(aligning MPRO at `0x100`), MPRO `0x100-0x4FF`.

| | blob0 | blob1 |
|---|---|---|
| COEF head | `7ff8 4000 0000 c000 8008 7ff8 73e0 d0a8` | same six, then `4158 8100` |
| MADRS nonzero | 12 | 9 |
| MADRS[0]/[2] | `0x1f00` / `0x101c` | `0x1f00` / `0x101c` |
| nonzero MPRO steps | 73 | 38 |
| last nonzero step | 81 -> LastStep 82 | 43 -> LastStep 44 |
| EWT (EFREG writes) | steps 45,47,79,81 -> EFREG[0..3] | steps 41,43 -> EFREG[0..1] |
| MRD/MWT steps | 15-37 (12), 59-65 (4), 73 | 11-33 (12) |

MAME `Step()` invariants the programs satisfy (`scspdsp.cpp:156-165`,
`248-265`): every executed step keeps `IRA <= 0x31` (else the interpreter
aborts the program), and every MRD/MWT step is odd (the "DoA inserts NOPs
on even" convention). Step 0 of both programs reads `IRA = 0x20` =
`MIXS[0]`, the slot-mix input bus.

## Effect-return table `0x605368`

Count word `3` (max index), four pointers: `0x60537a`, `0x60538a`,
`0x60539a`, `0x60538a` (entry 3 repeats entry 1). Blob0 (the boot's
choice) is `df cf ff ef` + 12 zero bytes -> slots 0-3 get a nonzero
EFSDL with EFPAN 31/15/31/15; slots 4-15 stay zero. This is exactly the
four EFREG channels blob0's program writes: the boot's program/return
pair is a matched 4-channel set (MAME returns `EFREG[i]` through slot
i's EFSDL/EFPAN at `scsp_ref.cpp:1371-1381`).

## Select handlers exist (bodies measured; live reachability open)

The byte-indexed jump table at `0x13de` carries `jmp (d16,pc)` stubs;
entries `0x2c-0x2f` are:

| entry | command | handler | body |
|---|---|---|---|
| `0x2c` | `0xAC` | `0x19c2` | save 32 slot bytes 0x0d (TL) -> `0x4250`, write 0xff, `index & 0x7f`; `0x7f` -> zero-fill `0x700-0xBFF`; `index > count` -> skip; else upload blob[index]; restore TL |
| `0x2d` | `0xAD` | `0x1a48` | fill 16 slot bytes 0x17 from the `0x605368` blob[index] |
| `0x2e` | `0xAE` | `0x1a76` | EFSDL set |
| `0x2f` | `0xAF` | `0x1a9a` | EFPAN set via the 128-byte LUT at `0x179c` |

Reachability: the `0x13ca` arm is entered with the class byte; its index
is `command & 0x7f`, so a `0xb0`-class command supplies only entries
`0x30-0x3f` - of which only `0x3a` (`0xBA` -> `0x1ac0` sample upload) has
a measured live caller. Entries `0x29-0x2f` are dormant by current
evidence; "runtime program switching" is a ROM capability, not a measured
live path. The C model correspondingly implements only the `0x3a` body
and refuses the a0-arm join as residue-wild - no silent divergence.

## C model impact

- `src/hardware/sound_board.c` already stores the window verbatim (boot
  mirror); behavior unchanged. Comments updated only ("wave" -> program
  upload, `0x402` -> delay-RAM config, per-slot volumes -> effect-return
  EFSDL/EFPAN, `0x405` reads -> MIBUF).
- The DSP itself (execution + audio) remains external-silicon/unmodeled;
  the software-visible contract is unchanged. The driver never reads DSP
  results: no real-code reference above `0xC00`, and the RINGBUF
  `0x600-0x6FF` has no real-code reference at all.

## Corrections

### v0804 ("complete 0x400-0x43F write set - no DSP program")

1. **Title/verdict.** The scan scope (`0x400-0x43F`) could not see the
   DSP window; the sustained traffic it looked for is the boot's
   `0x500`-byte upload to `0x700-0xBFF` plus the `0x402` ring config.
2. **`0x408` is not "dead letters".** `move.b $0016(a0),$0408(a5)` at
   `0x265c` is the drain's shared exit: `jmp` from `0x7ac` (idle) and
   `0x816` (processed), plus 10 main-loop branch tails - 12 xref sites,
   not zero. Every measured drain leg stops **at** `0x265c`, so the
   instruction is exactly one past the repo's measured boundary - live,
   unmeasured, not dead.
   Walk mechanics (ROM `0x3cc` init + `0x265c` file): stream records at
   `0x2000`, `0x20` stride, with `+0x0e = 0x10000 + i*0x2000`,
   `+0x12 = 2*i`, `+0x16 = 8*(i+1) & 0xff` (0 at record 31). The walk
   files each record's byte 0x16 into the MSLC latch (`0x408`), so the
   always-next processed record has its own slot selected for the
   `btst #7,$0409(a5)` poll at `0x227c` (MSLC readback bit 7 = CA bit 0
   of the selected slot's playback address, `scsp_ref.cpp:342-349`);
   byte 0x16 == 0 at record 31 is the walk's wrap marker.
3. **"No writes exist ... 0x41c-0x421 ... 0x423-0x43F" is wrong for
   `0x41C-0x428`.** ROM `0x2d2-0x2f2` does
   `movea.l a5,a0; lea $041c(a0),a0` then seven `move.w #imm,(a0)+`
   stores: `0x41c=0, 0x41e=0xc8, 0x420=0, 0x422=0x2c8, 0x424=0x88,
   0x426=0x48, 0x428=0` (`0x422` is then overwritten by the direct
   `0x40` at `0x300`). Invisible to a `(d16,a5)`-only scan because the
   store address comes from the `lea` + post-increment form. (The
   `0x41c-0x428` loop was already modeled in the C boot mirror as
   `scsp_block[7]`.)
4. **`0x402` function identified**: RBL/RBP delay-RAM config (see
   above), not an opaque constant.
5. **`0x405` "reads are waste"** -> MIBUF (MIDI-in) reads: the boot
   drain `0x306-0x312` plus the L3 receiver at `0x52a`.

### v0805 ("wave area 0x0700", "v0804 already closed ... no DSP-window traffic")

- `0x0700` is not a wave area: it is the DSP register surface, and the
  boot copy is a program upload (COEF/MADRS/MPRO), not sample data.
- The v0805 consequence that "v0804 already closed the write set with no
  DSP-window traffic" is superseded by this note.

## Residuals (deliberate, fail-closed)

1. DSP execution and audible output remain unmodeled external silicon:
   the delay ring (`0x38000-0x3FFFF` words), the EFREG mix and the MIXS
   feed are hardware paths the C does not execute. The software-visible
   contract (window bytes) is modeled verbatim.
2. The DSP input bus is fed by slots whose byte 0x15
   (ISEL bits 3-6, IMXL bits 0-2) is nonzero (MAME `SetSample`);
   the live i960-side values of that byte are not statically resolvable
   from the 68000 side (measured driver sources are 0 in all sampled
   preset banks).
3. MADRS mirror: under MAME the blob's zero pad words 96-127 would zero
   MADRS[0..15] through the alias; real-hardware aliasing is not
   established. The C stores bytes verbatim, which matches both readings
   at the software-visible boundary; not "fixed".
4. Live `0xAC/0xAD/0xAE/0xAF` selects remain unmeasured (see above).

## Test

`vf2_m68k_cdispatch_probe` (ctest) gains `test_scsp_dsp_program`
(`tests/m68k/test_cdispatch.c`): pins the table counts/pointers, both
blob geometries, `IRA <= 0x31`, odd-step memory ops, LastStep 82/44 +
the mid-burst-start equality, the EWT targets, the effect-return table
and blob0 arming, and the C boot mirror (full `0x700-0xBFF` window ==
blob0 verbatim, `0x402 == 0x0138`, slots 0-3 byte 0x17 ==
`df cf ff ef`). A future ROM set that moves any of these fails first.
