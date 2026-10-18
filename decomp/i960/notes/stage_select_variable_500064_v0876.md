# stage_select_variable_500064_v0876 — the arena/decor index is work-RAM byte 0x00500064

To capture stages other than the stone-towers VS arena, kazuyette needs the work-RAM
variable that chooses the loaded decor. Found from the ROM alone (no MAME), via the
shipped symbol `stage_disp`.

## Finding [MEASURED — disassembly]

**`0x00500064` is the arena/decor index** -- a byte, low 4 bits only (valid 0..15).

`stage_disp` (ROM `0x00020064`, shipped symbol, record 184) opens:
```
00020064  ldob 0x00500064, r12      ; stage index
0002006c  shlo 8, r12, r4           ; x256
00020070  ld   0x0006eea0(r4), r3   ; per-stage descriptor word (bit12/bit3 flags)
00020078  bbc  12, r3, 0x2008c      ; unused slot -> default
00020090  ldos 0x0006ee98(r4), r6   ; entry+0 short; 0xffff = unused slot
000200a0  cmpobe 0xffff, r6, ...     ; -> bail to default 0x8ff
```
So the index is scaled x256 and used to pick a per-stage descriptor in the table at
ROM `0x0006eea0` / `0x0006ee98` (data -- the disassembler reports "decode failed" there;
entries are `0x100` bytes apart). The body then submits that stage's decor model IDs
and camera/fog/fog-colour constants to the geometriser (the `st rN, (g11)[g12]`
display-list pushes). The stone-towers decor is model ids `0x494..0x4bd` ->
poly `0x3c1fac..0x3ca730` (the records geo-stage keeps, step5a).

The same byte is read at three more stage-setup sites -- `0x42510`, `0x42650`,
`0x42778` -- each `ldob 0x00500064, r5` then **`and 15, r5, r5`**, which pins the range
to **0..15**. No instruction stores `0x500064` with a direct immediate address, so the
writer is register-indirect (set from the match/select setup through a pointer).

## How to force a stage [for the MAME capture]

- `stage_disp` runs every fight frame (phase `0x50002a == 0x09`) and re-reads
  `0x500064`, so **poke the byte live during a fight**; it takes effect the next frame.
  No narrow setup window to hit, and the register-indirect writer won't clobber a
  mid-fight poke.
- VS always loads the stone towers, so the live value of `0x500064` in a VS fight **is**
  the stone-towers index; read it first, then try the other 0..15 values. A value whose
  table entry+0 short reads `0xffff` is an unused slot (stage_disp falls back to its
  `0x8ff` default).

## Nearby game-state landmarks (work RAM)

Phase `0x50002a` (07 select / 09 fight). P2 select cursors `0x515c86` / `0x515cc0`.
Config-block pointer `[0x0050016c]` -> service settings at `base+0x3340..` (MATCH
COUNT, DIFFICULTY, ENERGY MAX at `+0x3352` 1P / `+0x3354` VS, STAGE WIDTH -- see
`selector17_index4_game_assignment.md`; note STAGE WIDTH there is a service video
setting, NOT this arena index). Gameplay "stage" symbols `chk_ai_stage`/`chk_slow_stage`
are AI-level logic, unrelated to the arena. No ROM bytes committed.
