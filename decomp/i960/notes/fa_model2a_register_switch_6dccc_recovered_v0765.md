# v0765: 0x6dccc (`poly_or_model2a_register_switch`) recovered in C

**Slice 4 first-attempt slice of `advance_plan_v0764.md`.**
A v0734k-characterised gap-run sub-function, the smallest of the
four (88 B, 4 blocks). Recovers one of the standalone
Model 2A register helpers in the v0734k-characterised
`0x6cb0c..0x6dcb8` run.

## Function shape

```
sub_0006dccc: address=0x0006dccc end=0x0006ddd0 blocks=4 indirect=no
```

88 B in 4 blocks. The function does:

1. Read byte at `0x50002b` into r15.
2. If r15 == 16 or r15 == 17, branch to the entry's `ret`
   at `0x6dd24` (no-op path).
3. Otherwise:
   - read u32 from `0x59c318` into r14;
   - `cmpo 1, g0; addc 1, r14, r14` — net effect r14 += 1;
   - `addc 0, r15, r15` — net effect r15 unchanged;
   - mirror r14 to extended bus `0x1d03318` and `0x59c318`;
   - mirror r15 (the original byte) to extended bus
     `0x1d0331c` and `0x59c31c`.
4. Ret.

## Path inventory (3 legs in ctest)

| leg | trigger | exp ret ip | exp 0x59c318 | exp 0x1d0331c |
|-----|---------|-----------|--------------|----------------|
| A   | byte == 16           | 0x6dd24 | unchanged from pre | unchanged |
| B   | byte == 17           | 0x6dd24 | unchanged from pre | unchanged |
| C   | byte == 5, pre=0x41  | 0x6dd24 | 0x42 | 5 |

## Recovery decision points

- **Naming**: `poly_or_model2a_register_switch` is the working
  name per v0734k characterisation. The v0734i/v0734b rule is
  that no second-source signal is needed for `recovered-rom-anchor`
  when the disasm gives clear semantics, but a more specific
  name (e.g. `fighter_index_register_switch`) would require a
  second measurement. The current name is honest.
- **No-op simulation**: the i960 hardware branches to
  `block_0006dd20` which is just `ret`. The recovery simulates
  the whole branch-then-ret by setting `cpu->ip = 0x6dd24`
  directly (the byte after the entry's ret, which the caller
  supplies via `enter_procedure`'s g14).
- **cmpo + addc semantics**: `cmpo 1, g0` produces carry = 0
  (1 - 0 = 1, no borrow). `addc 1, r14, r14` therefore adds
  exactly 1. `addc 0, r15, r15` adds exactly 0 (carry is 0
  from the cmpo and not propagated past the first addc).
- **Standing assumption**: Model 2A address spaces for these
  pairs are real and routed by the bus observer. The recovery
  uses `vf2_model2a_write_u32` for all four stores; the
  observer routes correctly to `0x59c318` / `0x59c31c` /
  `0x1d03318` / `0x1d0331c`.

## What's in this slice (v0765) and what's NOT

- This slice recovers `0x6dccc` in C with three legs covering
  the no-op and increment paths.
- This slice does NOT wire `0x6dccc` into the per-step hook
  table — that is the same deferred slice as the rest of
  Slice 4 in `advance_plan_v0764.md`. The dormant-hook state
  documented in `per_step_hook_wiring_analysis_v0764.md`
  applies unchanged to this recovery.
- This slice does NOT recover the OTHER three parseable
  functions in the same v0734k gap run
  (`sub_0006cc00`, `sub_0006cf00`, and the shared
  epilogues at `0x6ce6c` / `0x6d10c`). Each is a
  separate focused slice.

## Test

`tests/recovered/test_model2a_register_switch_6dccc_native.c`
(vf2_model2a_register_switch_6dccc, ctest #57, 0.05 s).
Verifies all three legs: byte == 16 / byte == 17 / byte == 5
increment. Asserts:
- `status == VF2_OK`
- `cpu->ip == 0x6dd24` (post-ret)
- For legs A and B, the 0x59c318 register is unchanged from
  the pre-set value (the function does not write through).
- For leg C, `0x59c318 == 0x42`, `0x1d03318 == 0x42`,
  and the byte mirrors (`0x1d0331c` and `0x59c31c`)
  carry the original byte value 5.

## functions.csv row added

```
0x0006dccc,0x0006ddd0,poly_or_model2a_register_switch,
recovered-rom-anchor,rom-disassembly+unit,
Model 2A register-cluster switch: read byte at 0x50002b;
if 16 or 17, ret; else increment u32 at 0x59c318 and
mirror to extended bus 0x1d03318 plus the byte to
0x1d0331c; recovered v0765 from v0734k gap-characterised
sub_0006dccc; name provisional pending second-source
signal,
```

The end extent `0x6ddd0` is the address past the entry's
`ret` at `0x6dd20` (per the v0734a two-source rule: entry +
highest `ret` reachable through direct branches and
fall-through). The next real function entry
(`sub_0006ddd4`) starts at `0x6dd24` per the disasm
sequence.

## Validated

- `git diff --stat src/`: hybrid.c +200 LOC; new test.
- `git diff --stat include/vf2/hybrid/player.h`: +14 LOC
  declaration.
- `git diff --stat decomp/i960/functions.csv`: +1 row.
- `git diff --stat CMakeLists.txt`: +5 LOC test entry.
- ctest #57 `vf2_model2a_register_switch_6dccc`: PASS.
- ctest #56 `vf2_player_4ad40`: still PASS (regression check).
- ctest #55 `vf2_player_a154`: still PASS.
- ctest #54 `vf2_player_7ef0`: still PASS.
- ctest #53 `vf2_player_4421c`: still PASS.
- All wired-hook ctests (`vf2_callee_hook_counters`,
  `vf2_callee_hook_fires_native`, `vf2_f4_individual_release`)
  still PASS.
