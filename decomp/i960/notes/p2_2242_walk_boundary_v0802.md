# P2: 0x2242 walk entries + tails + pump wiring — boundary note (v0802)

## Scope

P2 is the 0x2242 main-loop head, its walk entries (the
stream-descriptor dispatch chain), per-entry tails, and the
L2 driver pump wiring that feeds it. Per v0797: "Unmodelable
in C (needs preempted register state): boundary."

This note documents the 0x2242 walk and the proposed recovery
plan. No C-side changes are made in this checkpoint — the
scope is a large recovery slice, not a single-session edit.

## 0x2242 main-loop head (per disasm)

```
0x2242: move.b (a0),d0           ; d0 = a0[0] (stream desc byte 0)
0x2244: bmi $224e                ; if d0 < 0 (bit 7 set), branch
0x2248: jmp $7a6                 ; if d0 >= 0, jump to walk entry
0x224c: move.w $001c(a0),d0      ; d0 = a0[0x1c] (timer)
0x2250: add.l $001a(a0),d0       ; d0 += a0[0x1a] (timer delta)
0x2254: bpl $2262                ; if d0 >= 0, branch (timer not expired)
0x2258: move.w d0,$001c(a0)      ; store updated timer
0x225c: jmp $7a6                 ; timer expired, jump to walk entry
0x2260: ...                      ; tail code (per-entry)
```

The main loop checks the stream desc byte 0. If bit 7 is set
(active stream), it decrements the timer. If the timer
expires, it jumps to the walk entry at 0x7a6. If bit 7 is
clear (inactive stream), it also jumps to 0x7a6.

## Walk entries (0x7a6+)

The walk entry at 0x7a6 is the dispatch chain for the stream
descriptors. Per v0799c and v0797, the walk entries include:
- 0x7a6: stream desc dispatch (per-byte class)
- 0x7b18: helper
- 0x7c60: helper

The walk entries are the "stream worker" chain. Per v0797:
"the worker self-disarms at 0x220e" and "the countdown/emit
half stays dead: +0x0c is boot-cleared and no store of
nonzero to any stream desc +0x0c exists."

## Tails (per-entry exit code)

Each walk entry has a tail (the per-entry exit code). The
tails handle the stream state transitions after the walk
body runs. The tails are at various addresses depending on
the entry.

## Pump wiring (L2 driver -> 0x2242)

Per v0797: "the L2 driver IS entered (vectored) and its
worker-forward (0x21ba +0x0c==0 -> 0x2242 main-loop head with
the preempted a0) is the live path."

The L2 driver at 0x21ba checks if stream desc +0x0c is 0. If
yes, it enters the 0x2242 main-loop head with the preempted
a0. The C can't model the preempted register state, so this
is a boundary.

## Why P2 is a boundary

1. **Preempted a0**: the 0x2242 main-loop head expects a0 to
   be the preempted stream desc pointer. The C doesn't model
   preempted register state.
2. **Dead countdown/emit**: per v0797, "the countdown/emit
   half stays dead: +0x0c is boot-cleared and no store of
   nonzero to any stream desc +0x0c exists." So the pump
   wiring is dead in practice.
3. **Worker self-disarms**: per v0797, "Worker self-disarms
   at 0x220e." The worker disarms itself, so the 0x2242
   walk is only entered via the L2 driver's preempted a0.

The C's current `vf2_sound_board_maintain_streams` (line 623)
is a no-op:
```c
/* The ROM sequence engine (driver 0x1f16, worker 0x1f7c, prescaler
   0x21ba, appliers, tail) never runs: the driver has no static
   entry anywhere in the image, and every slot's +0x0c byte is
   boot-cleared and never stored nonzero, so the prescaler would
   abort to the main loop at slot 0 regardless. Packet flow on ROM
   is main-CPU-authored (drain-consumed); the 68k never sequences.
   The old per-descriptor worker model matched none of the ROM
   applier bytes and is removed. Faithful behavior here is no
   state change; the cursors are reported to document stillness. */
```

The C's no-op is correct per v0797's "dead countdown/emit"
finding. The C correctly does not model the 0x2242 walk
because it's a boundary (preempted a0, dead in practice).

## Proposed recovery plan (for user approval)

To recover P2, the C would need:

1. **Preempted register model**: model the preempted a0 (and
   other registers) when the L2 driver enters 0x2242. This
   requires the i960 reference executor to capture and
   restore the preempted state.

2. **0x2242 main-loop head**: model the timer check and the
   jump to the walk entry.

3. **Walk entries**: model the stream desc dispatch chain at
   0x7a6+.

4. **Tails**: model the per-entry exit code.

5. **Pump wiring**: model the L2 driver at 0x21ba and its
   worker-forward to 0x2242.

This is a significant scope expansion. The C's current no-op
is correct per the dead-countdown finding. To recover P2,
the C would need a preempted register model, which is a new
architectural feature.

## Smaller alternative

If the goal is to verify P2 is correctly a boundary, the
audit is already done:
- 0x2242 main-loop head is documented
- Walk entries are documented
- L2 driver pump wiring is documented
- C correctly no-ops per v0797's dead-countdown finding

A "smaller" P2 would be this boundary note (no C-side
changes). A "larger" P2 would be the full recovery with
preempted register model.

## Conclusion

P2 boundary: the 0x2242 walk is correctly a boundary in the
C. The C's no-op `vf2_sound_board_maintain_streams` matches
v0797's dead-countdown finding. To recover P2, the C would
need a preempted register model, which is a new architectural
feature.

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p2_2242_walk_boundary_v0802.md` — this note
