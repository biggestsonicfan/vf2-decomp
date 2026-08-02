# v0.0.24 texture header decode

## Observed path

The helper prefix at `0x0004c180` consumes a little-endian bit stream from
`g3`. The observed child state at `0x00550080` is zero. Eight fields are
decoded with the original 16-bit reservoir rules: two 8-bit dimensions, one
8-bit code width, three 16-bit table values, one 4-bit nibble, and a final
16-bit table value.

The dimensions are rounded with `(raw + 1) >> 1`; their product and the other
fields are written to `0x0055c320` through `0x0055c340`. The prefix then
prepares the register contract consumed by the already recovered symbol-table
builder at `0x0004c3f0`.

## Architectural fidelity

The recovery preserves the exact reservoir post-state: `r13` holds the
remaining accumulator, `r14` the available-bit count, `r15` the next stream
address, `g0` the last shifted refill word, and `g11` the next 16-bit word. It
also reproduces all output registers and the final less comparison state.

The observed prefix is exactly 120 instructions with 36 bytes written and no
procedure calls or returns. It occurs four times.

## Validation

The public bridge test uses the first observed header stream and verifies all
ten output values, the complete bit-reservoir post-state, instruction
accounting, frame depth, final IP, and comparison control.

The exact VF2 2.1 ROM-backed `native-second-dispatch` validator executed 120
reference i960 instructions for every visit and reached complete CPU and Model
2 memory `MATCH`.

The strict totals are now 1,269,571 recovered and 1,251 interpreted
instructions across 180 recovered blocks and memory checkpoints, with 270 /
300 recovered calls and returns.
