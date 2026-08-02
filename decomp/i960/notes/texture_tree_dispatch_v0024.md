# v0.0.24 texture tree dispatch

## Observed path

The wrapper at `0x0004c544` saves 28 registers into a 112-byte stack area, prepares a 256-entry output table, calls the already recovered recursive tree expander at `0x0004c928`, restores the complete caller state, and initializes the byte decoder at `0x0004c6e0`.

The wrapper itself is exactly 63 instructions. It occurs four times, replacing 252 interpreted instructions while reusing `execute_texture_tree` rather than duplicating the recursive algorithm. The four former standalone tree-expansion checkpoints are now represented by four complete wrapper checkpoints.

## Architectural details

The stack stores remain visible in Model 2 work RAM, matching the original code. The nested procedure call, recursive calls, returns, maximum frame depth, tree writes and final decoder registers are all included in the bridge report. `shlo r6, 1, r12` is modeled with i960 operand order as `(1 << r6)`, followed by subtraction of one.

## Validation

The public test builds a two-leaf synthetic tree, verifies all 256 generated table entries through representative values, confirms the saved and restored registers, persistent stack writes, nested call/return accounting, and the exact byte-decoder contract.

The exact VF2 2.1 ROM-backed `native-second-dispatch` executed the complete original wrapper and nested tree for every visit and reached full CPU and Model 2 memory `MATCH`.

Strict totals are now 1,269,823 recovered and 999 interpreted bridge instructions, 180 recovered blocks and memory checkpoints, and 274 / 300 recovered calls and returns.
