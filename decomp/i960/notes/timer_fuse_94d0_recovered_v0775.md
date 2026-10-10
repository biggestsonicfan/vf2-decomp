# v0775 — `sub_000094d0` `timer_fuse` recovered

Recovered the 88-byte, 1-block, 22-instruction tiny
4-timer fuse function at `0x94d0..0x9528` (one past the
`ret` at `0x9524`). It packs the 4 Model 2A timer values
into a running "fuse" state and returns the low 16 bits
of the post-shift state.

## Disassembly (verified via `vf2i960 disasm ../../roms/vf2 0x94d0 24`)

```
000094d0  ld       0x00500098, r15       ; r15 = fuse_state
000094d8  shlo     20, 15, r14          ; r14 = 0xF00000 (timer base)
000094dc  ld       (r14), r13           ; r13 = timer[0]
000094e0  shlo     4, r13, r13          ; r13 <<= 4
000094e4  addi     r13, r15, r15        ; r15 += r13
000094e8  ld       0x00000004(r14), r13 ; r13 = timer[1]
000094ec  shlo     8, r13, r13
000094f0  addi     r13, r15, r15
000094f4  ld       0x00000008(r14), r13 ; r13 = timer[2]
000094f8  shlo     12, r13, r13
000094fc  addi     r13, r15, r15
00009500  ld       0x0000000c(r14), r13 ; r13 = timer[3]
00009504  shlo     16, r13, r13
00009508  addi     r13, r15, r15
0000950c  st       r15, 0x00500098      ; fuse_state = r15
00009514  shro     4, r15, g0           ; g0 = r15 >> 4
00009518  lda      0x0000ffff, r15
00009520  and      r15, g0, g0          ; g0 = (r15 >> 4) & 0xffff
00009524  ret
```

- 0 control flow (no branches, no calls)
- Reads the persistent "fuse" state at `0x500098` (in
  work_ram)
- Adds 4 timer values (each shifted into a unique 4-bit
  nibble position: 4, 8, 12, 16)
- Stores the accumulated value back to `0x500098`
- Returns `g0 = (new_fuse_state >> 4) & 0xffff`

## Why this works as a slot allocator

The function name `timer_fuse` is provisional. The
returned value is masked to 5 bits by the callers (e.g.
`and 31, g0, r5` at `0x542cc`), giving a 0..31 slot
index. The 4-timer accumulation effectively packs the
4 timer values' low 4 bits each into a 16-bit value,
which the callers interpret as a "next free slot" index
based on which timer just rolled over.

The Model 2A timers are 32-bit down-counters at
`0xF00000..0xF0000C` (4 timers × 4 bytes, per
`include/vf2/model2a.h:62`). They start at the
`VF2_MODEL2A_TIMER_RELOAD` value (0x07800f0f) and
decrement on cycle advance. The 4-timer fuse thus
provides a way to derive a "current time slot" from the
raw timer values without explicit time arithmetic.

## Recovery (`src/recovered/hybrid.c` end)

`vf2_hybrid_timer_fuse_94d0_execute` is a 1-to-1 C
translation. Reads the 4 timer values via
`vf2_model2a_read_u32`, accumulates per the disasm,
writes back to `0x500098`, and returns the masked value
in `g0`. No sub-calls.

## Test (`tests/recovered/test_timer_fuse_94d0_native.c`)

4 legs, each sets the 4 timer values and a pre-fuse-state
value, runs the recovery, and asserts the post-state
matches the disasm formula:

```
new_fuse = pre_fuse
        + (t0 << 4) + (t1 << 8) + (t2 << 12) + (t3 << 16)
g0 = (new_fuse >> 4) & 0xffff
```

| leg | t0 | t1 | t2 | t3 | pre | g0 |
|---|---|---|---|---|---|---|
| A | 0 | 0 | 0 | 0 | 0 | 0x0000 |
| B | TIMER_RELOAD | TIMER_RELOAD | TIMER_RELOAD | TIMER_RELOAD | 0 | 0xfeff |
| C | 1 | 2 | 3 | 4 | 0 | 0x4321 |
| D | 0 | 0 | 0 | 0 | 0xdeadbeef | 0xdb**ee** (low 16 of 0xDEADBE) |

ctest entry, 0.04 s, PASS.

## Where this fits

- Two callers: `0x1b780` (a slot-allocator) and
  `0x542c8` (inside the `0x5427c` polygon loader, which
  uses the returned g0 as a free-slot index in a 32-bit
  bitmap `r6`).
- 0x94d0 is in the same `0x9480` post-return area (the
  v0771 ret at `0x94cc` is one instruction before
  0x94d0). It sits between v0771's CRC16 hash and the
  start of the v0734k `0x4d2c0..0x4e808` gap run.
- Recovering this leaf unblocks 0x5427c (the slot
  allocator + polygon loader, 156 B, 8 blocks) and
  ultimately the 4 callers of 0x9480 (912-980 B each).

## `functions.csv` row

Added at line 26 (right after the v0774 row):

```
0x000094d0,0x00009528,timer_fuse,recovered-rom-anchor,rom-disassembly+unit,
88 B / 1 block / 22 instructions; reads persistent fuse
state at 0x500098; adds (timer[0]<<4)+(timer[1]<<8)
+(timer[2]<<12)+(timer[3]<<16) from Model 2A timers at
0xF00000..0xF0000C; stores back; returns
(new_state>>4)&0xffff; no sub-calls; callers 0x1b780
and 0x542c8; recovered v0775; extent 0x94d0..0x9528
(one past ret at 0x9524),
```

`end` is `0x9528` (one past `ret` at `0x9524`); the
v0734a two-source rule is satisfied (vf2i960 function
reports the same extent, and `vf2i960 disasm` confirms
`0x9524` is `0a000000 ret`).

## What is NOT yet done

- `0x5427c` (156 B, 8 blocks) is the next candidate. Its
  inner loop calls `0x94d0` (now recovered) for a slot
  index, then does 17-byte polygon record copies. With
  v0775 done, the refused sub-call on `0x5427c` is gone.
- The 4 callers of `0x9480` (0x6de3c, 0x6de6c, 0x6de90,
  0x6dec0) call `0x60330` (v0774 done), `0x5427c`, and
  `0x94d0` (v0775 done) before the hash. Once `0x5427c`
  is recovered, all sub-calls in the `0x9480` caller
  chain are recovered.
- This slice is `src/` delta of one new function, one
  new ctest entry, one new CSV row. The rest of
  `src/recovered/hybrid.c` is byte-identical.
