# v0167: remove the second-fighter 0x143e4 scheduler bridge

This checkpoint removes the remaining bounded i960 bridge from the second fighter's `fa_player` path at `0x000143e4 -> 0x00010dcc`.

The bridge had hidden several small architectural-state differences in the already recovered C path. ROM-backed boundary comparison isolated and corrected them:

- `0x1428c -> 0x142c0` now preserves the fighter-1 `g3` stream pointer as `0x00520df8` instead of the fighter-0 value `0x00520630`.
- `0x143e4 -> 0x143fc` reproduces the ROM's EQUAL condition state.
- `0x143fc -> 0x1abf4` reproduces the LESS condition state.
- `0x1abf4 -> 0x27d00` reproduces the GREATER condition state.
- `0x28184 -> 0x28268` reproduces the fighter-1 NONE condition state.
- `0x28780` now models the real `cmpobe`/`bg` interaction: types below 4 do not consume the `g3` byte stream. Type 3 writes zero; other types below 4 only advance `g2` by four bytes. Types above 4 retain the recovered indexed-stride behavior.
- The fighter-1 `0x28780 -> 0x2826c` reference cost is 1,054 instructions including its return, versus the fighter-0 recovered cost of 1,025.
- The final fighter-1 task boundary normalizes the condition state to NONE, matching the scheduler-return snapshot.

With those corrections the special fighter-1 bypass at `0x143e4` is removed and the full recovered helper chain runs through to scheduler return.

ROM-backed validation for fighter 1 remains exact:

- entry: `0x00013f08`;
- exit: `0x00010dcc`;
- instructions: 16,275;
- calls: 53;
- returns: 54;
- final snapshot: exact match against the i960 reference.

This eliminates the previous 3,765-instruction / 38-call / 39-return bridge at `0x143e4`.

No ROM bytes or generated snapshots are committed.
