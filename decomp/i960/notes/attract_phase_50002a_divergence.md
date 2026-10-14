# Attract boot divergence: native drives phase 0x50002a to 0x11, MAME holds 0x01

Date: 2026-10-14. Root-caused from reset against a MAME vf2 reference (live,
user-supplied). New in-tree tools: `vf2i960 boot-trace` and `vf2i960 read-words`
(tools/vf2i960/commands.c). No ROM-derived data committed.

## MAME ground truth (from reset)

`0x50002a` (the attract phase index): `0x00 -> 0x01` at frame 84 (ip `0xa970`),
`-> 0x02` at 723 (ip `0xa9a0`), `-> 0x03` at 724 (ip `0xacf4`), then frozen,
3D included. **Phase 0x11 never happens.** `0x5000a6` and `0x500704` stay 0.

## Native (pure interpreter from reset, `vf2i960 boot-trace roms/vf2 90`)

- f0: TGP start `0x980000 <- 0x80000000` then `0`, `0x980004` read `= 1` (matches
  MAME). 
- f16 ip `0xa970`: `0x50002a <- 0x01` -- **correct, same ip/value as MAME.**
- f17 ip `0x11e4`: `0x500704 <- 0x0f000000` (`[r6=0 r7=f0ffffff r11=01c00000]`).
- f17 ip `0xa76c`: `0x50002a <- 0x10`  (the observer logs post-write ip `0xa774`).
- f18 ip `0x58fb0`: `0x50002a <- 0x11`; then `0x10b58` writes `0x5000a6 <- 0xff`.

So the native reaches `0x01` correctly, then **one frame later forces it to
`0x10` and then `0x11`.** The divergence is at frame 17, not in the phase handler.

## The forcing branch: `frame_geometry_gate` @ 0xa748

```
a748  ld     0x00500704, r8
a750  bbs    26, r8, 0xa75c      ; if 0x500704 bit26 set -> 0xa75c
a754  bbs    2,  r8, 0xa75c      ; if 0x500704 bit2  set -> 0xa75c
a758  b      0xa800              ; else do nothing  <-- MAME's path (0x500704==0)
a75c  ldob   0x0050002a, r14
a764  cmpobe 17, r14, 0xa778     ; phase==17 -> geometry-commit path (0xa778)
a768  mov    16, r15
a76c  stib   r15, 0x0050002a     ; else FORCE phase = 0x10            <-- divergence
a774  b      0xa800
```

The per-frame gate, when `0x500704` bit 2 or bit 26 is set and the phase is not
yet 17, slams the phase to `0x10`. The phase-`0x10` handler (jump table entry 16
= `0x10a0c`) then advances it to `0x11` (`0x58fb0`), whose handler is `0x10b5c`
-- the worker that sets `0x5000a6 = 0xff` and is the documented attract blocker.

MAME keeps `0x500704 = 0`, so the gate falls through (0xa758) and the phase stays
`0x01`, where the phase-1 handler @ `0xa974` decrements the countdown `0x500024`
once per frame and only advances to `0x02` when it reaches 0 (frame 723).

## Why native `0x500704 != 0`: an uninitialised IO_CONTROL byte

`0x500704` is written at `0x11e4` as `~(r6 | r7)` (a status word is NOTed):

```
11d4  not     r7, r8
11d8  st      r8, 0x00500700
11e0  andnot  r6, r8, r3        ; r3 = ~r7 & ~r6
11e4  st      r3, 0x00500704    ; 0x500704 = ~(r6 | r7)
```

At f17, `r6 = 0`, `r7 = 0xf0ffffff`, so `0x500704 = ~0xf0ffffff = 0x0f000000`
(bit 26 set). `r7` is built in the function at `0x1100`+ from the IO_CONTROL
region at base `r11 = 0x01c00000` (`VF2_IO_CONTROL_BASE`):

- low three bytes `0xffffff` come from the host input ports at relative
  `0x10/0x12/0x14` (no input pressed -> `0xff` each) -- correct.
- the top byte is `0xf0 | (read(0x01c0001c) & 0x0f)`:

```
1118  ldob  0x1c(r11), r3       ; r3 = *(0x01c0001c)
1120  and   15, r3, r3          ; r3 &= 0x0f
1124  shlo  4, 15, r13          ; r13 = 0xf0  (hardcoded high nibble)
1128  or    r13, r3, r3         ; r3 = 0xf0 | (byte & 0x0f)
112c  shlo  24, r3, r3
1130  or    r3, r7, r7          ; r7 bits 24..31 = 0xf0 | nibble
```

A byte read of `0x01c0001c` is not an input port (`model2a_read_input_port`
returns UNSUPPORTED for relative 0x1c), so it falls through to the generic
IO_CONTROL view and returns `io_control[0x1c]`, which `vf2_model2a_initialize`
never sets -> **0**. Hence the top byte is `0xf0`, `r7 = 0xf0ffffff`, and
`0x500704 = 0x0f000000`.

For `0x500704 = 0` (MAME) we need `r7 = 0xffffffff`, i.e. the top byte `0xff`,
i.e. `read(0x01c0001c) & 0x0f == 0x0f`. On real hardware this maps to the
pulled-up / open-bus state `0xff` -- consistent with the neighbouring IO_CONTROL
bytes the init already sets to `0xff` (relative `0x02/0x04/0x06/0x10/0x16/0x18`;
`0x1c` is simply missing from that list).

## Fix PROVEN, but gated on a celsowm decision

Setting `machine->io_control[0x1c] = 0xff` in `vf2_model2a_initialize` (matching
the neighbours `0x02/0x04/0x06/0x10/0x16/0x18`) makes the **interpreter boot
match MAME exactly**, verified with `vf2i960 boot-trace roms/vf2 820`:

```
0x50002a: 0x00 -> 0x01 @f16  ip 0xa970   (MAME ip 0xa970)
          0x01 -> 0x02 @f656 ip 0xa9a0   (MAME ip 0xa9a0; countdown span 640 vs MAME 639)
          0x02 -> 0x03 @f657 ip 0xacf4   (MAME ip 0xacf4)
then frozen at 0x03, never 0x10/0x11; 0x5000a6 = 0, 0x500704 = 0.
```

The instruction pointers are identical to MAME's and the phase-1 countdown span
(640 frames) matches MAME's (639); the absolute frame numbers differ only because
our from-reset vblank-injection cadence is not MAME's wall-clock vblank.

**Why it is not yet committed:** the one-byte fix changes the reachable boot path
(the phase holds at 0x01 instead of racing to 0x11), so the recovered native
runtime -- whose boot BRIDGE BLOCKS hardcode post-states captured from the old,
buggy path -- no longer validates. `vf2i960 native-sixth-dispatch` fails with:

```
Bridge block interrupt-initial-cluster post-state address mismatch:
  IP=0x00000c74/0x00000c78 depth=2/2
Native first-dispatch validation failed during second-scheduler-pass:
  unsupported operation
```

i.e. with correct I/O the real code at 0x0c74 goes somewhere the recovered
bridge (built for the 0x11 path) does not expect, and later a scheduler task that
only the corrected path reaches has no recovered block. So applying the fix
requires **re-validating / re-recovering the boot bridges (src/recovered/boot/*
and the interrupt-initial-cluster bridge) against the corrected I/O**, and
regenerating the committed `native-*-dispatch` snapshots. That reshapes celsowm's
recovered-C corpus and the Phase 4 lifting corridor built on it -- a maintainer
decision, so per the session goal work stops here for celsowm's call.

Open sub-question for that decision: confirm the IO_CONTROL power-on value at
relative 0x1c is exactly `0xff` (pulled-up/open-bus) against the 315-5649 I/O
datasheet or a MAME `0x01c0001c` read, rather than inferring it purely from the
`0x500704 == 0` requirement.

## The fix also unblocks attract -> 3D (part 2), measured

`boot-trace` now also counts writes into the geometry program region
(`0x804000`+, TGP model records) and the copro FIFO (`0x884000`+). From reset,
1100 frames, pure interpreter:

| metric                     | no fix (phase 0x11, stuck) | fix (phase 0x03) |
|----------------------------|----------------------------|------------------|
| geometry-program writes    | 11,520                     | 171,706 (~15x)   |
| FIFO command writes        | 13,643                     | 446,986 (~33x)   |
| 0x884000 reads             | 3,294                      | 93,209  (~28x)   |
| final 0x50002a             | 0x11                       | 0x03             |
| final 0x5000a6             | 0xff                       | 0x00             |

So the one-byte fix does not merely correct the phase byte: it moves the game out
of the attract self-test wait (the 0x11 worker 0x10b5c that sets 0x5000a6=0xff)
into the state that actively submits geometry -- MAME keeps 0x50002a at 0x03
"3D included", and our interpreter, with the fix, reaches that same 0x03 state and
pours model records + FIFO commands (15-33x the stuck baseline). This is the
attract->3D game-flow transition that has blocked Phases 5/6/8/9. Whether those
submitted meshes render into a visible 3D frame still depends on the TGP geometry
pipeline (Phase 6 / G2), but the game-flow side is now reached.

This sharpens the celsowm decision: the fix is a correct one-byte hardware-model
change that unblocks the project's central attract->3D blocker; the only cost is
re-validating the recovered boot bridges + regenerating the native-dispatch
snapshots against the corrected I/O.

## Reproduce

```
vf2i960 boot-trace roms/vf2 90          # phase + TGP + 0x500704 trace from reset
vf2i960 read-words roms/vf2 0xa6f8 18   # the phase jump table
vf2i960 disasm    roms/vf2 0xa748 12    # frame_geometry_gate
```
