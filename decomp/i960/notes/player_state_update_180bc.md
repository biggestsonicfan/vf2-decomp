# Player state update at 0x000180bc

The supported Virtua Fighter 2 Version 2.1 program ROMs from the current
evidence set were reconstructed with the manifest's `LOAD32_WORD` layout.
The four i960 program chips match the project manifest CRCs:

- `epr-18385.12`: `78ed2d41`
- `epr-18386.13`: `3418f428`
- `epr-18387.14`: `124a8453`
- `epr-18388.15`: `8d347980`

No ROM data is stored in the repository.

## Call-chain position

The recovered sixth-dispatch pose corridor now reaches `0x00014418`.
That wrapper immediately calls `0x000180bc`, returns at `0x0001441c`, sets
player base flag bit 7, stores the flag word, and returns to its outer caller.

`0x000180bc` is a complete leaf routine ending at `0x00018140`.

## Recovered semantics

For the player structure in `g7`, the routine:

1. sign-extends the 16-bit value at `+0x26`;
2. when state bit 6 in `+0x1a4` is set, adds the sign-extended 16-bit bias
   at `+0x812`;
3. stores the low 16 bits at `+0x5b4`;
4. updates base flag bit 8 using state bit 3, mode bit 7 at `+0x810`, and,
   on the non-shortcut paths, the unsigned comparison
   `(+0x1aa) <= ((+0x800) >> 1)`;
5. when state bit 0 and base flag bit 4 are both clear, stores a zero byte
   at `+0x6d8`;
6. returns to `0x0001441c`.

The `bbc`, `bbs`, and `cmpoble` instructions are COBR operations in the
project executor and do not modify `compare_result` or arithmetic-control
condition bits. The native recovery therefore leaves both unchanged.

Depending on the branch combination, the leaf executes 14 through 25
instructions including its `ret`.

## Native bridge coverage

The tail dispatcher now supports all of these guarded runs without embedding
an observed RAM snapshot:

- `0x180bc -> 0x1441c`: complete leaf;
- `0x14418 -> 0x1441c`: CALL plus complete leaf;
- `0x1441c -> outer return`: four-instruction wrapper tail;
- `0x14418 -> outer return`: complete wrapper plus leaf in one run.

The implementation plans every branch before the first write, respects
`max_steps`, keeps trace-enabled runs on the architectural interpreter, and
uses the real i960 procedure enter/return helpers so frame depth and
procedure-call/return counters remain architectural.

The recovered branch equations, writes, and instruction counts were
cross-checked against an independent address-level simulation over 100,000
randomized state combinations.
