# `fa_game_info` `0x18644`: cross bit2/bit4 (`0x104 <-> 0x110`) v0038

## Scope

This note records ROM-backed recovery of the ordered fighter-state pair
`0x104 <-> 0x110` in child procedure `0x00018644`.

The states are:

- `0x104`: state bit 8 + bit 2;
- `0x110`: state bit 8 + bit 4.

Both physical orientations were validated, with countdown byte 0/1 and mode
bit 6 clear/set.

## ROM vector

The enumerated pure-ROM caller-to-task vector for both orientations is:

```text
cd0/m0  197
cd0/m1  202
cd1/m0  194
cd1/m1  199
```

This is vector class 1 in `game_info_18644_state_vectors.csv`.

## Semantic admission

The recovery opens only the exact `0x104 <-> 0x110` composition in the two
existing fail-closed gates that rejected it:

1. the initial bilateral state-composition gate;
2. the later threshold gate reached on the non-bit1 path.

No mask-based or vector-class generalization was introduced.

## Isolated differential proof

The reusable `diagnose_game_info_pair.py` tool was used at both child return
sites:

- first caller invocation: return `0x000164b0`;
- second caller invocation: return `0x000164c4`.

For both orientations, both countdown values and both mode-bit-6 values, the
final candidate matched the ROM oracle in all 16 isolated probes:

```text
architectural snapshot: MATCH
instructions:            delta 0
calls:                   delta 0
returns:                 delta 0
interrupt entries:       delta 0
interrupt returns:       delta 0
```

The first semantic-only candidate already matched CPU/memory and procedure
counters in 16/16 probes. Its remaining instruction deltas were measured
explicitly. The second call showed the same role swap previously observed in
other asymmetric corridors: the logical `r7/r8` orientation seen by the late
accounting block is reversed relative to the physical fighter-state
orientation. The final accounting table therefore uses call-site-specific
corrections with the `0x164c4` orientation mapping swapped.

## Strong chained proof

The reusable `validate_game_info_pair.py` tool then executed the complete
chain:

```text
native child #1
 -> caller ROM
 -> native child #2
 -> ROM tail
 -> scheduler return 0x00010dcc
```

All eight cases were exact:

```text
0x104 -> 0x110: 4/4 MATCH
0x110 -> 0x104: 4/4 MATCH
TOTAL:           8/8 exact
```

Each match includes CPU/mutable-memory snapshot equality plus exact serialized
instruction, call, return, interrupt-entry and interrupt-return counters.

## Validated recovery

Functional recovery commit validated by the ROM-backed proof:

```text
a5f3ee5637eb33170fba01023b7c3287ade83cd1
```

The matrix rows `0x104,0x110` and `0x110,0x104` are therefore marked
`recovered_exact=yes`.
