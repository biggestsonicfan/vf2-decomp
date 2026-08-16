# Selector17 bit-7 index 10: VS DIAGRAM

ROM slot `0x0005fef8` selects entry `0x0005f234` (`a4 = 0x8a`). The handler has two states.

## State 0

Builds the static VS diagram: LOSS/WIN headings, eleven fighter abbreviations and names, border glyphs, and the TEST-button exit prompt, then advances `a5` to 1. Measured handler corridor: 1,650 instructions, 31 calls, 32 returns.

## State 1

The live body iterates the 11 x 11 matchup table rooted at `base + 0x36a4`. For each pair it reads the two 16-bit counters, computes the displayed percentage with the i960 floating conversion/divide/multiply/round sequence, and renders it with `0x7ff0`. It then sums each fighter row, stores totals at `0x00500280` and fighter ids at `0x00500244`, sorts those parallel arrays ascending by the computed score, renders rank values 1000..11000, and rebuilds the diagram borders. `0x8440` is the one-tile primitive `*(u16*)g9 = 0x8000 | g0`.

Measured handler corridors: normal refresh 36,729 instructions / 1,436 calls / 1,437 returns; TEST exit 51,001 / 1,452 / 1,453. The TEST check is at the end of the refresh, so exit still recomputes/redraws the full diagram before shared teardown at `0x5f140`.

The recovered bridge now implements both states, including the parallel-array sort, numeric formatting, border construction, parent-menu restoration, and measured CPU post-state.

## Border table

The horizontal border loop consumes the full 61-entry ROM table at `0x5fdb4..0x5fea4`, one tile per column 1..61. It begins with `0x13`, uses `0x0f` at the internal intersections, and ends with `0x12`; this is not a 15-entry pattern expanded in groups.
