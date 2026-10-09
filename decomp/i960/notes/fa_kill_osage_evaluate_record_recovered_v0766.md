# v0766: 0x65838 (`kill_osage_evaluate_record`) recovered in C

**Slice 4 v2 / second function recovery of `advance_plan_v0764.md`.**
The only `recovered-rom-anchor` row in `functions.csv` (line 95)
that had no C translation. 108 B in 7 blocks, two confirmed
callers via `vf2i960 xrefs` (`0x65828`, `0x65830`).

## Function shape

```
kill_osage_evaluate: address=0x00065838 end=0x000658a4 blocks=7 indirect=no
```

108 B, 7 blocks. The function decides whether to "kill" an
osage worker record and increments a global kill counter
when it does. Shape:

1. `r7 = 0`.
2. `r3 = *(g7 + 0xc)`. Compare to magic `0x6428c` (the
   `kill_osage_record` task-handler constant for this worker
   pool). Mismatch -> no-path.
3. `r3 = *(g7)`. Check bit 0 clear and bit 2 set. Otherwise
   no-path.
4. `r7 = *(g7 + 0x128)`. Check `r7 + g6 >= 0x4268`. Otherwise
   no-path.
5. **YES path**: set bit 3 of `*(g7)`, increment global
   counter at `0x500164`.
6. **NO path**: clear bit 3 of `*(g7)`, accumulate `r7` into
   `g6`.
7. Ret.

The work takes `g7` as a work-RAM struct base (i960 global
register 7, `cpu->registers[16 + 7]`).

## Path inventory (5 legs in ctest)

| leg | trigger | exp ret | effect on *(g7) bit 3 | effect on *(0x500164) | effect on g6 |
|-----|---------|---------|------------------------|-----------------------|----------------|
| A   | *(g7+0xc) != 0x6428c | 0x658a4 | unchanged (clear) | unchanged | += 0 |
| B   | *(g7) bit 0 set     | 0x658a4 | unchanged (clear) | unchanged | += 0 |
| C   | *(g7) bit 2 clear   | 0x658a4 | unchanged (clear) | unchanged | += 0 |
| D   | r7 + g6 < 0x4268    | 0x658a4 | unchanged (clear) | unchanged | += r7 |
| E   | all conditions met  | 0x658a4 | bit 3 SET          | += 1     | unchanged |

A through D verify the four early-exit NO paths. E verifies
the YES path's two effects.

## Recovery decision points

- **Naming**: `kill_osage_evaluate_record` follows the
  existing `functions.csv` row (line 95). The `record`
  suffix distinguishes from the container that calls this
  function twice (the wrapper at `0x65800..0x65838`).
- **g7 access**: per v0758 / v0759 / v0760 pattern, g0..g15
  live at `cpu->registers[16..31]`. So `g7` is
  `cpu->registers[16 + 7] = cpu->registers[23]` and `g6`
  is `cpu->registers[22]`. The `VF2_I960_G0_REGISTER + N`
  macro is the idiomatic form.
- **Threshold `0x4268`**: kept as an `UINT32_C(0x4268)`
  literal in the comparison. The disassembly confirms this
  is a literal in the `lda 0x00004268, r6` opcode. Naming
  the threshold (e.g. "min-kill-time") would require a
  second measurement per v0734i / v0734b naming policy.
- **Magic `0x6428c`**: same approach — kept as a literal.
  This is the constant for the work-pool record type
  (`0x6428c` is the entry of `sub_0006428c` itself, per
  v0734k's characterisation of the gap run
  `0x6428c..0x657dc`).
- **`r3 < r6`**: i960 `cmpobl r3, r6, label` means
  "if r3 < r6, branch" (B = below, L = likely, but in
  i960 cmpobl is "compare ordered and branch low"). The
  C translation uses `<` correctly.

## Test

`tests/recovered/test_kill_osage_evaluate_record_native.c`
(vf2_kill_osage_evaluate_record, ctest #58, 0.02 s).
Asserts all 5 legs end-to-end.

## What's in this slice (v0766) and what's NOT

- This slice recovers `0x65838` in C with all 4 NO paths
  and the YES path verified.
- This slice does NOT wire the function into the per-step
  hook table — same deferred slice as the rest of v0764.
- This slice does NOT recover the wrapper at
  `0x65800..0x65838` (52 B, 3 blocks) that calls this
  function twice with different `g7` contexts. The
  wrapper's body computes `g6 = (r15 - 0x4268 - 18) / 25`
  — a "duration measurement" idiom that ties to the
  threshold literal in this function.
- This slice does NOT recover `kill_osage_evaluate_record`
  as a real recovered dispatch (it has 2 callers but
  neither is on the post-29414 dispatcher corridor
  measured at v0730). It remains a stand-alone unit test.

## functions.csv

The row already existed at `0x65838..0x658a4` with
`recovered-rom-anchor` status. The status remains
`recovered-rom-anchor` and the source remains
`rom-disassembly` (matching v0752/v0755f/v0753 sibling
recoveries). The note in the row is updated to mention
the v0766 unit test.

## Validated

- `git diff --stat src/`: hybrid.c +181 LOC; new test.
- `git diff --stat include/vf2/hybrid/player.h`: +13 LOC
  declaration.
- `git diff --stat CMakeLists.txt`: +5 LOC test entry.
- `git diff --stat decomp/i960/functions.csv`: +0 LOC row
  update (status/source unchanged; note augmented).
- ctest #58 `vf2_kill_osage_evaluate_record`: PASS.
- ctest #57 `vf2_model2a_register_switch_6dccc`: still PASS.
- ctest #56 `vf2_player_4ad40`: still PASS.
- ctest #55 `vf2_player_a154`: still PASS.
- ctest #54 `vf2_player_7ef0`: still PASS.
- ctest #53 `vf2_player_4421c`: still PASS.
- All hooked callee tests (cf04, 43888, 439ac, 29598)
  still PASS.
- vf2_native_fifth_dispatch (#122) still PASS at 14.90 s.
- vf2_native_sixth_dispatch (#123) still PASS at 15.09 s.
- vf2_f4_individual_release (#146) still PASS at 17.45 s.
