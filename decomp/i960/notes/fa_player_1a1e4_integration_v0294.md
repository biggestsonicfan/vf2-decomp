# v0294: integrate 0x1a1e4 selector setup into 0x19ef8

## Change

`hybrid_execute_player_19ef8` no longer hand-writes the observed
`0x1a1e4` record-setup stores. It now calls the recovered semantic
interpreter `player_selector_execute_setup()` (from
`src/recovered/player_selector_setup.inc`), then still performs the
post-interpreter `+0x1a8 = selector` and `+0x1aa = 1` stores that belong
to the outer `0x19ef8` caller, then the unchanged `0x26ef0` scratch
expand and the same register/`0x1428c` endpoint.

Guards are unchanged: `0x505`/`0x284` selector, `packed_value == 0x200`,
profile bytes, `player_flags` bits 5/6/21/23, `+0x1a4 == 0`, runtime bit
20, board bit 16, selector bits 13/14, `branch_byte` bit 6.

## Why this is safe

For the accepted `0x505` corridor the interpreter's initial zero stores,
`base_flags`/`final_state_flags` (equal to `packed_value` when bit 21 is
clear) and opcode-1 record bytes reproduce the manual block exactly.
The interpreter additionally saves `+0xbd4`/`+0xc30` (old state); on the
PUNCH corridor those locations are already zero, so Work RAM is
unchanged at the differential endpoint.

## Validation

- `cmake --build build --config Debug` (warnings clean)
- `ctest --test-dir build -C Debug --output-on-failure` → **56/56**
- PUNCH: `vf2cycles --cycles 320` → **320/320 MATCH**, 12946 blocks,
  **14.962.620** insns, both `0x0001645c`

## Not done here

- Selector-value guard still closed; relaxing it needs remaining caller
  state predicates recovered semantically (integration step 3).
- `0x26ef0` / `0x27130` stay independent boundaries.
- `0x19ef8` flag-bit siblings and `0x29414` non-zero path still
  measured/deferred separately.
