# selector17 bit-7 index10 — VS DIAGRAM

ROM slot `0x0005fef8` points to `0x0005f234`; flagged phase index is `0x8a`.

The handler has two `a5` states through the jump table at `0x0005f248`:

- state 0: `0x0005f250`
- state 1: `0x0005f590`

## State 0

State 0 is deterministic UI setup. It renders `LOSES(%)`, the eleven fighter labels/names (AKIRA, JACKY, SARAH, KAGE, LAU, JEFFRY, PAI, WOLF, SHUN, DURAL, LION), six diagram glyphs, and `PUSH TEST BUTTON TO EXIT.`, then increments `0x005000a5` from 0 to 1.

Measured isolated ROM corridor: **1650 instructions / 31 calls / 32 returns**.

The recovered-C bridge implements this state completely.

## State 1

State 1 rebuilds the diagram every frame. The ROM:

1. iterates an 11x11 matchup matrix using the fighter-order table at `0x0005fd08` and records rooted at `base + 0x36a4`;
2. reads two 16-bit counters for each ordered pairing and converts them to a percentage-like score (`10000 * losses / total` when both counters are present, with explicit zero/empty handling);
3. renders the matrix cells;
4. computes one aggregate score for each fighter and stores fighter indices at `0x00500244` and scores at `0x00500280`;
5. bubble-sorts those 11 `(fighter, score)` pairs by score;
6. renders rank values and the diagram/bracket decorations from constant tables at `0x0005fd34`, `0x0005fd78`, and `0x0005fdb4`;
7. exits through the shared TEST MENU teardown at `0x0005f140` when `(input & 0x04000104) != 0`.

Measured isolated ROM corridors:

- idle/update: **36729 instructions / 1436 calls / 1437 returns**;
- TEST exit: **51001 instructions / 1452 calls / 1453 returns**.

State 1 remains intentionally unsupported in recovered C until the numeric/tile rendering semantics of helpers `0x00007ff0` and `0x00008440` are translated without relying on ROM execution. This keeps the bridge strict rather than substituting an approximate visualization.
