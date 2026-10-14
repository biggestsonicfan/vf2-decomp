# Attract geometry-gate chain: the 3D blocker is upstream of the geometry machinery

Date: 2026-10-14. Evidence-only analysis (no recovered C changed). Reproduces
from the ROM via `scripts/geometry_census.*` plus the interpreter frame loop
from the cold sixth-dispatch snapshot (`out/cold.vf2snap`, not committed).

## Question

Why does the attract corridor never render 3D? `vf2i960 geometry-census` shows
the reachable stream is only ring bookkeeping and copro-FPU camera scalars
(`223.2f`/`172.8f`/`600.0f`), with no renderable TGP opcodes. Where is the gate?

## `frame_geometry_gate` @ 0x0000a748 (disassembled)

```
a748  ld     0x00500704, r8
a750  bbs    26, r8, 0xa75c      ; bit 26 set -> continue
a754  bbs    2,  r8, 0xa75c      ; bit 2  set -> continue
a758  b      0xa800              ; else SKIP (no geometry this frame)
a75c  ldob   0x0050002a, r14
a764  cmpobe 17, r14, 0xa778     ; require byte 0x50002a == 17
a768  mov 16,r15 ; stib r15,0x0050002a ; (else set 16 and skip at a774)
a778  ldob   0x005000a6, r3
a780  cmpobne 0, r3, 0xa800      ; require byte 0x5000a6 == 0, else SKIP
a784  ...                        ; DEEP path: call 0x8ef0 (commit), clear
                                 ; 0x50081c bits 0/1, zero 0x500708/704/700
```

So the per-frame geometry **commit** runs only when all three hold:

1. `*0x00500704` has bit 2 **or** bit 26 set (a geometry-request state word);
2. byte `*0x0050002a == 17`;
3. byte `*0x005000a6 == 0`.

## Measured over the attract corridor (600 frames, interpreter from cold snap)

- `frame_geometry_gate` (0xa748) and `main_geometry_prefix` (0xa014) run once
  per frame; the gate's end `0xa804` and deep entry `0xa784` are **never**
  reached -- the gate always branches to `0xa800` (skip).
- At the gate each frame: `0x50002a == 17` holds (300/300), but
  `0x500704` bits 2/26 are **clear** and `0x5000a6 != 0`, so condition 1 and 3
  both fail.
- `polygon_object_submit` (0x7c60), the display-list builder (0x61ba0) and
  `video_command_submit` (0x4b410) are **never** reached in attract.

## Forcing experiment (diagnostic only, not a change)

Forcing `0x500704 |= (1<<2)` at the gate each frame: condition 1 passes, 2
passes, but condition 3 (`0x5000a6 == 0`) still fails -> still skips, deep path
never runs. Additionally forcing `0x5000a6 = 0`: the deep commit path `0xa784`
**is** now reached (18 frames), and the flow diverges (the gate is then hit only
35/300 frames). But `0x7c60` and `0x61ba0` are **still never reached**.

## Conclusion

`frame_geometry_gate` is the per-frame geometry **commit**, and it is correctly
idle in attract: there is nothing to commit. Object submission and the
display-list builder (`0x61ba0 -> 0x7b18 / 0x7c60`) are **upstream** and never
entered, because the attract state machine never transitions into a 3D-scene
state that would populate a display list. The geometry machinery is ready and
gated correctly; the blocker is the **attract -> 3D-scene state transition**
(Phase 5 game-flow recovery), not the geometry pipeline (Phase 6).

This localizes the "advance past attract" prerequisite recorded in
`docs/NATIVE_WINDOWS_PLAN.md` Phase 6 Status: the next recovery target is
whatever sets `0x500704` bit 2/26 and clears `0x5000a6` as part of entering the
demo/gameplay state, and whatever upstream logic builds the display list that
reaches `0x61ba0`/`0x7c60`.
