# geo_pipeline_step2k_ram9_addrmap_v0848 — RAM9 zones for body/shadow matrices

**Geometriser pipeline, Step 2k.** kazuyette: since Lau's body reads drift +104px
while the game shows him immobile, the fault is in our *reading* of the body
matrices, not the game. Before touching i960/TGP code, map where in RAM9 the body
and shadow matrices live: per object of frame 20 give the RAM9 address of its 0x0b
matrix command and its 0x01 object command and the 32 KiB zone (relative to the
readstart); are the bodies reached by a jump to another zone; and across frames,
do the body matrices stay at the same place while the readstart rotates through the
4 zones (which would mean our snapshot captures another frame's data)?

RAM9 is 0x20000 bytes = four 32 KiB zones (0x8000 each); the readstart cycles
z0/z1/z2/z3 across frames (double/quad buffering).

## Tool

`geo_camtest.py addrmap <ram9.bin> <readstarts.txt> [pose]` (stdlib). Re-walks with
address tracking: per object it records the byte address of the 0x0b command that
set its matrix and of its own 0x01 command, flags every jump (and whether it
crosses a zone), and tabulates across all frames whether the body/decor matrix
addresses stay fixed or follow the readstart zone.

## Result — the stale/shared-zone hypothesis is REFUTED [MEASURED]

**Frame 20 (readstart 0x00000, zone 0):** every object's matrix and object command
is in **zone 0** — all 26 decor, all 30 shadow, all 55 fighter. There are 16 jumps
and **all 16 stay inside zone 0**; **no cross-zone jump**, so the bodies are *not*
reached by a jump into another zone. Decor, shadows and fighters are interleaved in
one contiguous list (decor at 0x006ec.., shadows 0x00fcc.., fighters 0x01888..).

**Across all 30 frames:** the whole display list is a **complete self-contained
copy in the current readstart zone**, with identical internal offsets every frame —
only the zone (high) bits rotate with the readstart:

| readstart zone | decor matrix | body matrices |
|---|---|---|
| z0 | 0x006ec | 0x01888..0x02798 |
| z1 | 0x086ec | 0x09888..0x0a798 |
| z2 | 0x106ec | 0x11888..0x12798 |
| z3 | 0x186ec | 0x19888..0x1a798 |

The body matrices sit at the **same offset inside whichever zone is current**, i.e.
they rotate correctly with the readstart alongside the decor and shadow matrices.
They are **not** parked at a fixed/shared absolute address, and nothing jumps out to
a fixed region to fetch them.

**So our snapshot reads each frame's body matrices from that frame's own,
correctly-buffered zone, co-located with the decor and shadow matrices.** We are not
reading body data from a different frame via a shared zone or an out-of-zone jump.
The decor (same zone, same buffering) projects correctly, so the zone bookkeeping is
sound.

## What this leaves open — write timing, not zone selection

The address map cannot see *when* within a frame the i960/TGP wrote each matrix,
only where. Since the body matrices are correctly placed and buffered, any body
drift is in the **data written into that zone** (or in a write-vs-readstart timing
race), not in our zone choice. That is exactly what kazuyette's prepared capture —
the i960/TGP writes to RAM9 tagged with the frame number — will settle: whether the
body matrices captured for frame n were written for frame n, or are a late/early
write belonging to an adjacent frame. The two excluded records 0x581d50 / 0x1a9b54
remain excluded.

## Next

Await kazuyette's RAM9 write-trace (coprocessor + i960 writes with frame numbers).
With it, check per body matrix whether its last writer wrote it for the displayed
frame or an adjacent one (a write/readstart timing race), vs. the game genuinely
writing drifting body matrices (which MAME would then also render, re-opening the
question of the ground-truth reading).

No C changed this step (geo_camtest.py addrmap mode only); gcc and clang build
clean, vf2_geo_display + the geometry differential tests pass; geo_camtest.py
parses. No capture/screenshot data committed.
