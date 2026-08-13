# Player `0x16504` pose recovery

This note records the clean-room recovery of the observed player pose corridor `0x00016504 -> 0x0001769c -> 0x00014418`.

## Status

The owned observed corridor is now fully executed by recovered C. It no longer invokes the architectural i960 executor internally and no longer depends on the former `P165` compressed replay/patch blob.

The architectural instruction count remains exactly `1,690` instructions, including the function's final RET.

## Recovered structure

The original routine is an unrolled 16-joint pipeline rather than sixteen unrelated routines. The C recovery makes that structure explicit.

Each joint has:

1. a selector-specific preparation block;
2. a common joint result tail;
3. a call to the common table-driven helper at `0x176a0`.

All sixteen joint tails are represented by one common implementation. Each tail emits the same joint command, reads a returned three-word tuple, stores it at `player + 0x1f4 + selector*12`, and invokes the recovered `0x176a0(selector)` helper.

## `0x176a0` helper

The helper is recovered generically for selectors `0..15`.

It reads the selector's count and list from MAIN_DATA, walks the list in the ROM's descending order, emits the table-driven coprocessor RPCs and, when state bit 31 permits, stores returned tuples into the player's `+0xd00` table.

Selector zero's intentional `count=0 / null-list` representation is supported explicitly rather than treated as invalid.

## Preparation families

The sixteen preparation blocks were reduced to semantic families instead of retaining the assembly's unrolled duplication.

### Simple vector/angle preparations

Selectors `3, 4, 6, 7, 11, 13, 14` share one descriptor-driven implementation. Their vector source is `player + 0x80 + selector*12`; they emit the common vector and angle commands and derive the side-dependent angle selector from `0x3a00/0x3b00 + selector*12`.

### Diagnostic preparations

Selectors `1, 2, 5, 8, 9, 12, 15` share a second descriptor-driven implementation. It additionally models the diagnostic ring at `0x005001e4 / 0x0090e000`, optional player/global triplets and the observed diagnostic commands `0x36006c6c`, `0x36806d6d` and `0x37006e6e`.

### Selector 0

Selector zero performs the pose-wide coprocessor setup, captures the nine-word global result block at `0x00501024`, handles the fighter flag/gate dependent vector setup, and emits the common angle state before entering joint zero.

### Selector 10

Selector ten is a distinct object-to-geometry bridge. The recovery:

- resolves the live object descriptor;
- transforms sixteen fixed triplets through the observed coprocessor service;
- stores them into the player pose table;
- expands the object's variable geometry records to the live geometry stream;
- selects local versus player-table triplets by the record indices; and
- preserves the ROM's loop condition state.

The variable record count is bounded explicitly rather than permitting unchecked structural input.

## Final tail

The final `0x175f4 -> 0x1769c` tail is also native C. It:

- commits the previous/current pose triples;
- computes planar X/Z deltas;
- applies the two global mode gates;
- invokes the recovered two-component norm semantics used elsewhere in the player path;
- reproduces the `0.7` threshold comparison and state-bit gates; and
- performs the architectural RET to `0x14418`.

This removed the last internal `vf2_i960_run()` dependency from the owned pose boundary.

## Replay removal

The obsolete `src/recovered/player_i960_bridge_pose_blob.inc` file was deleted. The remaining pose data file contains only entry/final architectural guards and CRC acceptance values; it is not an execution replay.

## Acceptance gates

The recovery preserves the exact known post-state CRCs:

- geometry: `0xae84ffcb`;
- coprocessor port RAM: `0xe8ba2142`;
- work RAM: `0xc4116ecd`;
- buffer RAM: `0x2ab1791e`.

It also checks the exact final architectural register state, process/arithmetic controls, compare result, frame depth and total procedure accounting.

The pose wrapper composes through outer runs: entering at `0x16504` no longer requires the caller's stop address to be exactly `0x14418`. After the recovered pose completes, the remaining run budget is delegated to the previous hybrid layer. This is important because the full player task uses a farther outer stop.

## Differential validation

The fully recovered pose implementation is exercised inside the task-level sixth-dispatch run, not merely through an isolated exact-stop helper.

Repository CI passes with GCC release, Clang release, and Clang ASan/UBSan.

The V2.2 ROM-backed `native-sixth-dispatch` remains an exact match with the fully-C pose active:

- repeated-frame reference instructions: `7,404,917`;
- continuous recovered instructions: `8,675,741`;
- final CPU state: MATCH;
- final memory state: MATCH.

No ROM image, runtime snapshot or proprietary memory dump is committed to the repository.
