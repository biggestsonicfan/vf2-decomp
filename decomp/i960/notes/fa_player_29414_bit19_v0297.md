# v0297: recover fa_player 0x29414 bit-19-set paths

## Verdict

`hybrid_execute_player_29414` now natively recovers the measured
bit-19-set siblings for selector types 6/8/10, in addition to the
existing bit-19-clear float tail.

Compare semantics on the loaded `+0x1aa` halfword are **unsigned**, and
the disassembly operand order is inverted from the naive reading:

| Instruction | Effective test | Destination |
|-------------|----------------|-------------|
| `cmpobl 20, r12, 0x294f8` | `20 < r12` (i.e. `r12 > 20`) | path B |
| `cmpobge 10, r12, 0x29538` | `10 >= r12` (i.e. `r12 <= 10`) | float tail |

Fall-through at `0x294ac` is therefore the closed window
`11 <= r12 <= 20` (path A). Path A is reachable and was previously
assumed unreachable.

## Measured paths

Park: `player-14288-rt` via `vf2probe --set-ip 0x00029414`.
Type 6 is the base count; type 8 is +2 dispatch compares; type 10 is -1.

### Float tail (`bit19 clear` or `bit19 set` and `window <= 10`)

Store `(r9 * scale - scale)` at `+0xc50`.

| Selector | Body insns (to `ret`) | `+0xc50` (scale 2.0) |
|----------|----------------------|----------------------|
| 6 | 13 clear / 16 set | `0xbd75c280` |
| 8 | 15 clear / 18 set | `0xbe23d708` |
| 10 | 12 clear / 15 set | `0xbd75c280` |

### Path A (`11 <= window <= 20`)

1. `r11 = window - 10`
2. Indexed stores at `(g11)[g12]` with measured scale 0 (`g11 + g12`):
   `0x0b801717`, then `r11`
3. Reload `r11`, `r9 = r9 + r10 * float(r11)` (`mulr`/`addr`)
4. If board bit 5 clear: `+0x18a`/`+0x17c` halfwords `+= r13 * r11`
5. Float tail store at `+0xc50`

The `r10 * r11` term uses the integer `r11` bits as a float, so for
`r11 = 1..10` the addend is a denormal and `r9` is effectively unchanged.
Recovery still performs the IEEE ops.

| Selector | Board bit5 clear | Board bit5 set |
|----------|------------------|----------------|
| 6 | 33 insns, halfword add | 25 insns, no halfword |
| 8 | 35 | 27 |
| 10 | 32 | 24 |

Halfword deltas (signed 16-bit add of a 32-bit product):

- type 6/10 `r13 = 0xffffff00` (`-256`); window 11 → add `-256` → `0xff00`
- type 8 `r13 = 0xfffffe67` (`-409`); window 11 → `0xfe67`

### Path B (`window > 20` unsigned)

No `+0xc50` store. Early returns:

1. board bit 5 set → ret (type6 body 13)
2. `(+0x614 & 0x9000) == 0` → ret (type6 body 17)
3. `50 < window` → ret (type6 body 19; measured with `window = 0xffff`)
4. else add type-constant `r8` to `+0x18a`/`+0x17c`, ret (type6 body 26)

| Selector | r8 | window 21 + mask halfword |
|----------|----|---------------------------|
| 6/10 | `0xfffff800` (`-2048`) | `0xf800` |
| 8 | `0xfffff000` (`-4096`) | `0xf000` |

`window` is compared unsigned, so a negative stored halfword (`0xffff`)
takes path B and hits the `50 < window` early return.

## Unrecovered / out of scope

- Constant set at `0x29454` (`r9 = 0x3f6147ae`) is not reachable from
  this entry's type dispatch (only 6/8/10 are selected). Left unmeasured.
- Other selector values still take the type-0 zero path (unchanged).
- Registers other than `r14` are not restored beyond `return_procedure`
  local-frame behavior.

## Tooling

No new tooling. Measurement reused `vf2probe --set-ip` from v0296.

## Pins

- PUNCH **320/320 MATCH**, 12946 blocks, **14,962,620** insns, both `0x1645c`
- ctest Debug **56/56**
- unit test `test_player_29414_type_paths` extended for path A/B/float-set

## Next

- Wire the existing `player_execute_28178_stream` into
  `hybrid_execute_player_post_29414` so the `0x28178 → 0x14400` segment
  is no longer pure `hybrid_execute_interpreted_until`.
- `0x19ef8` flag-bit siblings remain blocked by the parked-snapshot
  `0x2704c` interpretive fault.
- Geometry helpers after `0x28780` remain the next non-blocked frontier.
