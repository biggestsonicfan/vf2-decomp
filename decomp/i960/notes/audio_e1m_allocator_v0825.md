# E1m: 0xf12 voice allocator past the 90-LOW door (9/10 arc)

Status: `sound_board_run_allocator_0f12` wired into the
90-LOW walk end + 90-low leg extended through it +
drain-90 head LOW repin + two unit-leg conversions green
in-file (whole binaries, 0 FAIL). Full suite in the commit
validation.

## Recovery

$151a is the absolute voice cursor (init 0x2800; records
0x2800..0x29f0 stride 0x10, sentinel 0x2a00): 0x2a00 wraps
to 0x2800 with write-back, then up to 32 records scanned
for +2 == 0. Found: cursor = found + 0x10 (write-back),
refuse at the 0xfac setup authoring (S3: voice fill +
volume math + engine -- not entered). None-free (dbf
exhausts): refuse at the 0xf50 reaper scan (separate slice;
it key-offs and continues). No residue question exists --
the cursor is RAM, every op is move.b/compare on bytes.
Insane cursors fail closed per-read (silicon would bus-read;
unmeasurable either way).

Wired into the 90-LOW walk end ONLY. Loop-B's three 0xf12
exits still park (their S2d legs own them) -- wiring them is
one call plus leg extensions, explicitly not this slice
(recorded in the allocator comment, not silently left).

## Differential (90-low fixture + allocator staging)

$151a = 0x2800 staged (zeroed board leaves 0 -- the garbage
path both sides would agree on, silicon-identical but
meaningless), voice0 busy (+2 = 0x40, the 90 path never
reads voices before the allocator, so the staging is clean):

- C: UNSUPPORTED @0xfac, walk cells intact, cursor 0x2820
  (skipped voice0 to voice1), held.
- S: 0xfac in 67 steps (51 walk + 16 allocator: wrap-check,
  two scan iterations, cursor write, bra), identical cells +
  cursor, full-RAM (drain exclusions) + SCSP equality.
  First-try match.

## Ripple (all per-evidence, same slice)

- drain-90 head LOW (S stops pre-walk at 0xa3e/36,
  untouched): C pin 0xf12 -> 0xfac with $151a staged (found
  voice0 on the zeroed voices); HIGH untouched (0xf12,
  allocator not wired -- the shared-leg assert is
  conditional again, E1k pattern).
- Unit legs x2 (init stages $151a = 0x2800, all voices free):
  handler 0xf12 -> 0xfac + cursor 0x2810 asserts.

## Past the new door

0xfac setup authoring (voice fill, SCSP init, volume
mulu/lsr/not, jsr 0x1062, engine) and the 0xf50 reaper
(+9-scan steal with key-off) are S3 territory. The 90-LOW
chain is now: head -> walk -> allocator -> setup door.
