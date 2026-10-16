# E1o: Loop-B arrivals wired into the allocator + exhausted leg (9/10 arc)

Status: four `run_loopb`/`setup_scan` 0xf12 exits wired to
`sound_board_run_allocator_0f12` + all leg extensions +
new `test_allocator_exhausted` green in-file (whole
binaries, 0 FAIL). Full suite in the commit validation.

## Correction first (S3-core status)

The voted "S3 0xfac core path" targets work that is already
done: volume math, both pickers, slot init, pitch,
envelope, sample tail and engine form one unbroken native
chain (`setup_author_high/low` -> `sample_tail` ->
`engine_key`) with NO refuse pins -- proven by the
production leg (full 90 + setup-author + slot program to
0xcb2, full-RAM equality) and E1c. S3's genuinely-open
remainder was this slice (Loop-B wiring, promised in E1m)
plus the reaper.

## Wiring (one call per site, E1m pattern)

- `run_setup_scan` tail (0x9ba bra 0xf12).
- `run_loopb` 0x91c (ch-0xf non-b6/b8), 0x926 ($152e < 5),
  0x950 (scan miss).
- 90-LOW walk end (E1m, unchanged).
Every ROM bra to 0xf12 now enters the allocator. Handler
pins move 0xf12 -> 0xfac (found) on every affected leg; the
0xcb2 "arm" leg is untouched (never arrives).

## Differentials (print-first, constant-tail structure)

Allocator-found tail = +10 steps everywhere (one scan
iteration + cursor write + bra); skip-one-more = +16:

| leg | before | after |
|---|---|---|
| drain-90h (x3) | 69 / 71 / 72 | 79 / 81 / 82 |
| loopb miss / setup / setup9ba | 203 / 73 / 201 | 213 / 83 / 211 |
| 9ba two / skip | 616 / 203 | 626 / 219 |
| 90-low (E1m) | 51 @0xf12 | 67 @0xfac |

Cursors: 0x2810 (found voice0) on all but skip (0x2820,
voice0 stays 0x40) and alloc-x (0x2800 untouched). Full-RAM
(drain exclusions) + SCSP equality on every ROM leg; the
drain-90 split legs keep their pre-walk S stops with C-only
allocator pins ($151a staged pre-snapshot for the
production path -- the zeroed-board garbage path both sides
would agree on is silicon-identical but meaningless).

## New: exhausted leg (second exit proof)

32 busy voices (+2 = 0x40): dbf exhausts, cursor untouched
(0x2800), refuse at the 0xf50 reaper with the packet held;
S arrives 0xf50 in 246 steps with full equality (neither
side wrote past the walk). The allocator contract is now
both-exits-proven.

## Unit legs

Two Loop-B 0x926 legs (init boards: cursor 0x2800, voices
free): handler 0xf12 -> 0xfac + cursor 0x2810 asserts.

## Remaining sound work

0xf50 reaper body, 0xb76 LOW fill (unreachable from any
recovered path -- likely permanent), live gameplay writers
(all hunts negative), DSP-absence sweep.
