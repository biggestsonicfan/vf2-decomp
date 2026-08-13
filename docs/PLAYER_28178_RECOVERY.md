# Player `0x28178` record-stream recovery

This note records the semantic recovery of the player stream corridor rooted at `0x00028178`, returning through `0x0001abf8` and `0x0001b530` to `0x00014400`.

## Previous limitation

The first native bridge recognized one exact 21-byte sequence: three seven-byte records beginning with opcode `0x12`. It then hard-coded the observed effect of the first two records and assumed the third record's threshold stopped interpretation.

That was a useful differential foothold, but it modeled one byte string rather than the record language implemented by the ROM.

## Stream interpreter

The code at `0x1abf8` is a bytecode/record interpreter.

Each record begins with:

```text
+0  opcode       u8
+1  threshold    u16 little-endian
```

Interpretation stops when:

- `opcode == 0`; or
- the live player counter is less than `threshold`.

A due record is dispatched through the ROM jump table at `0x1b910`. Unknown due opcodes remain an explicit architectural fallback boundary.

The recovered parser is transactional: it parses and validates all due supported records before applying the first mutation.

## Opcode `0x12`

The recovered handler at `0x1b2f4` has the complete seven-byte format:

```text
+0  0x12
+1  threshold      u16
+3  slot           u8
+4  target         u8
+5  target_count   u16
```

The slot selects:

```text
player + 0x6c4 + slot * 6
```

whose state is:

```text
+0  duration       u8
+1  target         u8
+2  current        s16
+4  step           s16
```

The handler compares `target_count` with the **low byte** of the live player counter, matching the ROM's `ldob +0x1aa(g7)`.

### Future target

When `target_count - low8(counter) > 0`:

```text
duration = target_count - low8(counter) + 1
target_value = target << 8
step = (target_value - current) / duration
```

The handler stores `duration`, `target` and the signed 16-bit `step`.

The current recovery rejects durations above 255 before mutation instead of guessing the intended behavior after the ROM's byte-sized `stob` wraps the timer.

### Already-due target

When `target_count - low8(counter) <= 0`, the ROM takes the immediate path:

```text
duration = 1
target = record.target
current = record.target << 8
```

The existing step field is not rewritten by this branch.

## Multiple records and same-slot composition

The parser no longer assumes two active records or distinct slots. It accepts a bounded sequence of due opcode-`0x12` records and simulates their effects in program order during planning.

Consequently a later record targeting the same slot observes the state produced by the earlier record, exactly as the ROM interpreter would.

The parser then stops naturally on an opcode-zero terminator or on the first future threshold and stores the resulting cursor back to `player + 0x82c` / `g4`.

## Two-slot interpolation tail

After record interpretation, `0x1b4e8` advances the two fixed interpolation slots 0 and 1.

The C recovery distinguishes all three observed control-flow forms:

- inactive timer: no state change;
- timer decrements to zero: clear duration/target and clear the low byte of current while preserving its high byte;
- timer remains active: `current += signed step`.

The final `cmpinco` condition state and the architectural RET at `0x1b530` are preserved.

## Dynamic instruction accounting

Instruction accounting is derived from the recovered control flow rather than fixed at the original 128-instruction snapshot:

- due opcode-`0x12`, interpolating path: 34 instructions including interpreter dispatch;
- due opcode-`0x12`, immediate path: 31;
- opcode-zero terminator: 2;
- future-threshold stop: 10;
- tail slot inactive: 5;
- tail slot expires: 11;
- tail slot continues: 13.

The original observed stream still resolves to exactly **128 instructions**.

## Synthetic regression matrix

`tests/recovered/test_player_28178_stream.c` constructs work-RAM-only states with no proprietary ROM attached. If the semantic bridge rejects these states and falls through to the architectural executor, the tests necessarily fail because there is no program ROM to execute.

The CI therefore proves that the semantic parser itself handles cases the original 21-byte hard-code could not:

1. one due record followed by a future threshold;
2. the immediate opcode-`0x12` branch and timer expiration;
3. two records targeting the same slot;
4. no due record while two already-active slots advance, including one expiration.

## Integration validation

The semantic stream tests pass under:

- GCC release;
- Clang release; and
- Clang ASan/UBSan.

The V2.2 ROM-backed `native-sixth-dispatch` also remains exact with the parser composed into outer player-task runs:

- repeated-frame reference instructions: `7,404,917`;
- continuous recovered instructions: `8,675,741`;
- final CPU state: MATCH;
- final memory state: MATCH.

No ROM image or runtime snapshot is committed to the repository.
