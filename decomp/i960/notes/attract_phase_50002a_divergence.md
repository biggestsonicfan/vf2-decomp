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

## Candidate fix (next slice)

Initialise `io_control[0x1c]` (and audit its open-bus neighbours) so the IO_CONTROL
status byte at relative 0x1c reads `0xff` like the others. Expected result: at
f17 `r7 = 0xffffffff`, `0x500704 = 0`, `frame_geometry_gate` falls through, the
phase stays `0x01`, counts down in the `0xa974` handler and advances `0x01 ->
0x02 -> 0x03` as in MAME, never reaching `0x10`/`0x11`. Validate with
`boot-trace` (phase trajectory) and the full suite. Whether the IO_CONTROL
power-on/open-bus value is exactly `0xff` is a hardware-model fact worth
confirming against the 315-5649 I/O datasheet / MAME before locking in.

## Reproduce

```
vf2i960 boot-trace roms/vf2 90          # phase + TGP + 0x500704 trace from reset
vf2i960 read-words roms/vf2 0xa6f8 18   # the phase jump table
vf2i960 disasm    roms/vf2 0xa748 12    # frame_geometry_gate
```
