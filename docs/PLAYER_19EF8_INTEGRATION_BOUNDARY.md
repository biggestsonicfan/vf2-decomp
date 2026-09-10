# Player `0x19ef8` integration boundary

The selector-setup language interpreted by `0x1a1e4` is now recovered independently from the outer `0x19ef8` caller.

This distinction is intentional:

- selector bytecode support is a property of the generic `0x1a1e4` interpreter;
- `0x19ef8` still owns separate pre/post-selector control flow and later `0x26ef0` / `0x27130` behavior;
- therefore a selector that parses successfully must not automatically be classified as a fully recovered `0x19ef8` path.

The integration sequence is:

1. **Done (v0294).** Replaced the manual selector-`0x505` setup block in `hybrid_execute_player_19ef8()` with `player_selector_execute_setup()`, retaining the caller's `0x505`/`0x284` guard and the post-interpreter `+0x1a8`/`+0x1aa` stores. PUNCH stays `320/320` MATCH and ctest `56/56`.
2. The same V2.2 differential endpoint is required (`0x1428c`, 1652/1805 insns, 4 calls / 4 returns).
3. Remove the selector-value guard only after the caller's remaining state predicates are expressed semantically.
4. Keep `0x26ef0` / `0x27130` as explicit independent recovery boundaries rather than adding selector-specific exceptions.

This prevents the old anti-pattern of treating a caller-state limitation as an unsupported selector or fighter family.

## Remaining caller guards (still fail-closed)

`hybrid_execute_player_19ef8` still returns `VF2_ERROR_UNSUPPORTED` when:

- `player_state_flags (+0x1a4) != 0`
- `runtime_flags (0x500068) bit 20`
- `board_flags (0x508000) bit 16`
- `packed_value != 0x200`
- selector profile bytes wrong for `0x505` / `0x284`
- `player_flags` bits **5, 6, 21, 23**
- `selector` bits 13, 14
- `branch_byte` bit 6

Sibling measurement from the parked `player-14288-rt` snapshot is blocked: the reference interpretive path faults at `0x2704c` even on baseline, so those guards cannot be relaxed without a live hybrid-context drive.
