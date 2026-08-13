# Player selector setup (`0x1a1e4`) recovery

This note records the clean-room recovery of the selector-configuration language interpreted by the i960 routine at `0x0001a1e4`.

## Scope boundary

`0x1a1e4` is **not** a fighter-family switch and selector `0x505` is not a privileged protocol value. The routine is a generic selector bytecode interpreter.

The surrounding `0x19ef8` caller still contains separately recovered/partial control flow and later calls into `0x26ef0` / `0x27130`. Unsupported states in that outer caller must therefore not be described as unsupported selector bytecode.

The selector language itself is recovered for opcodes `0..17`.

## Selector tables

For a selector `s` (`0 <= s <= 0x1fff`):

```text
data   = *(u32 *)(0x0200d34c + s*4)
table  = *(u32 *)(0x02120004 + s*4)
```

The first eight bytes at `data` are fixed selector metadata. The bytecode begins at:

```text
data + 8
```

Selector zero takes the ROM's no-data reset/finalization path.

## Fixed preamble

Before dispatching bytecode, the routine clears the transient selector state including:

```text
+0x802
+0xa00
+0x812
+0x83c
+0x844
+0x841
+0x823
+0x850
+0x818
+0x854
```

For non-zero selectors it also:

- loads the table value into `+0x800`, `+0x80a` and `+0x80c`;
- snapshots old `+0x1a4` to `+0xbd4`;
- snapshots old `+0x804` to `+0xc30`;
- loads the first four data bytes as the selector state word;
- copies data bytes `4..7` to `+0x810/+0x811/+0x814/+0x815`;
- applies the selector/board-specific flag transforms visible in the ROM;
- clears state bit 21;
- resolves the bit-8/bit-15 conflict by moving bit 15 to bit 16; and
- stores the resulting state word to `+0x1a4`.

The board-variant condition is the live bit 6 of the byte reached through `0x0050016c + 0x3351`, not a compiled fighter profile.

## Bytecode dispatch table

The recovered jump table is:

| Opcode | ROM handler | Record size | Recovered semantic effect |
|---:|---:|---:|---|
| 0 | `0x1a39c` | terminator | stop at current cursor |
| 1 | `0x1a408` | 3 | write `+0x802/+0x803` |
| 2 | `0x1a420` | 7 | configure `+0x808/+0x809/+0x80a/+0x80c`, phase/state bit |
| 3 | `0x1a554` | 14 | copy 13 selector bytes into the extended state fields |
| 4 | `0x1a5c4` | 17 | main motion/stance setup, derived `+0x822`, `+0x1230` counter |
| 5 | `0x1a760` | 10 | compact motion setup, derived `+0x822`, increment `+0x1230/+0x1234` |
| 6 | `0x1a7f0` | 7 | copy six motion bytes |
| 7 | `0x1a828` | 3 | write `+0x812/+0x813` |
| 8 | `0x1a398` | 1 | consume terminator and stop |
| 9 | `0x1a840` | 15 | copy `+0x830..+0x837`, set `+0x5be`, scale float into `+0x5cc`, set `+0x81a` |
| 10 | `0x1a8d4` | 6 | retain first record pointer at `+0x838`, increment `+0x83c` |
| 11 | `0x1a8f0` | 11 | configure packed `+0x844/+0x6be` and six adjacent bytes |
| 12 | `0x1a9d8` | 3 | write `+0x841/+0x842` with selector override for `0x2e3` |
| 13 | `0x1aa18` | 3 | write `+0x84d` and `+0x858` |
| 14 | `0x1aa30` | 15 | piecewise float interpolation into signed byte `+0x822` |
| 15 | `0x1ab04` | 3 | consume record, no state mutation |
| 16 | `0x1ab0c` | `2 + 2*N` | save variable record pointer to `+0x854`, select byte into `+0x800` |
| 17 | `0x1ab2c` | 7 | consume record, no state mutation |

## Opcode 2

Opcode 2 configures two bytes and two 16-bit selector values, sets phase `+0xa00 = 1`, and sets state bit 1 in `+0x1a4`.

The ROM contains selector/board-specific numeric overrides. They are represented as data-driven membership checks in the semantic handler, rather than as a separate fighter implementation.

## Opcodes 4 and 5

Both are motion-state setup records.

The derived signed-byte value written to `+0x822` is based on:

```text
base = (s8)player[0x69c]
value = ((base*5 + 100) * scale) / 100
```

where `scale` is the signed record byte (with the exact selector/board overrides used by opcode 4).

Opcode 4 increments `+0x1230`; opcode 5 increments both `+0x1230` and `+0x1234`.

## Opcode 9

Opcode 9 reads a binary32 value from the record, multiplies it by the live binary32 global at `0x0050a000`, forces the sign bit, and stores the resulting bits to `player + 0x5cc`.

Non-finite numerical states are rejected rather than assigning guessed hardware behavior.

## Opcode 11

Opcode 11 loads a packed word, applies the ROM's selector-specific bit-15 toggle set, applies the player-side bit-6 toggle, then stores:

```text
packed            -> +0x844
packed & 0x7fff   -> +0x6be
```

and copies the remaining six bytes to `+0x848/+0x849/+0x84a/+0x84b/+0x84e/+0x84f`.

## Opcode 14

Opcode 14 is a piecewise float interpolation that ultimately writes a signed byte to `+0x822`.

The semantic implementation reproduces:

- the two possible interpolation segments;
- orientation-independent clamping for positive or negative float ranges;
- float division/multiplication; and
- the i960 `cvtri` rounding mode selected by arithmetic-control bits `31:30`.

All four rounding modes used by the architectural executor are modeled:

```text
0  nearest, ties to even
1  floor
2  ceil
3  truncate toward zero
```

This is why the runtime-facing setup API accepts the live `arithmetic_control` instead of assuming round-to-nearest.

## Transactional parsing

The parser performs a bounded planning pass before applying the fixed setup or the first opcode mutation.

It validates:

- selector range;
- data/table pointers;
- each opcode;
- fixed/variable record size;
- cursor overflow; and
- a maximum of 64 opcode records before a terminator.

A structurally invalid or unterminated stream is therefore rejected before selector state is mutated.

## Finalization

On completion the semantic path stores the resulting cursor to:

```text
player + 0x82c
player + 0x6d0
```

It also reproduces the player-side bit-6 sign inversions/toggle and the board-specific `0x3e4/0x3e5` cleanup at `+0x1218`.

## Validation status

The selector language is tested independently with synthetic MAIN_DATA and work RAM; no proprietary ROM is embedded in the tests.

The regression matrix includes selectors other than `0x505`, fixed-size opcodes, variable opcode 16, float opcode 9, piecewise opcode 14, and a 64-record unterminated stream that must fail before mutation.

**Important:** completing this bytecode interpreter does not by itself make every `0x19ef8` caller state recovered. The next integration step is to replace the manual selector-`0x505` setup inside `hybrid_execute_player_19ef8()` with this semantic interpreter, validate equivalence, and only then relax the caller's remaining guards.
