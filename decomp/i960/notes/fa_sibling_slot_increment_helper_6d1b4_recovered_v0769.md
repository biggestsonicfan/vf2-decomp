# v0769: 0x6d1b4 (`sibling_slot_increment_helper`) recovered in C

**Slice 4 v5 of `advance_plan_v0764.md`.** A pure-clone of v0768
for the second of the two slot-increment siblings in the v0734k-
characterised gap run `0x6cb0c..0x6dcb8`.

## Function shape

```
sub_0006d1b4: address=0x0006d1b4 end=0x0006d1ec blocks=3 indirect=no
```

56 B, 3 blocks. **Structurally identical** to `sub_0006d10c`
(v0768) — the only difference is the array offset and the
mirror addresses:

| constant           | v0768 (0x6d10c) | v0769 (0x6d1b4) |
|--------------------|------------------|------------------|
| array offset       | 0x3520           | 0x393c           |
| ext-bus mirror     | 0x1d03520+r7*4   | 0x1d0393c+r7*4   |
| Model 2A mirror    | 0x59c520+r7*4    | 0x59c93c+r7*4    |

The body is otherwise identical: `r7 = max(20, g0/1920)`,
read bus pointer from `*(0x50016c)`, increment slot at
`bus_ptr + offset + r7*4`, mirror to two slots.

## Path inventory (3 legs in ctest)

Same shape as v0768 legs (large g0 / small g0 clamped / edge
case), with the v0769 constant offset:

- **A**: g0 = 1920*100 → r7=100, source at array[100], dests
  become 0x1001 (and source stays 0x1000 — read-only).
- **B**: g0 = 100 → r7=20 (clamped), source at array[20],
  dests become 0x421.
- **C**: g0 = 5883 → r7=20 (still clamped), source at
  array[20], dests become 0x100.

## Recovery decision points

- **No new logic**: this is a pure-clone of v0768 with two
  constants changed. The disasm-to-C mapping is identical
  to v0768; only the literal `0x393c` (vs `0x3520`) and
  the two mirror address bases (`0x1d0393c` and `0x59c93c`)
  differ.
- **Two-sibling pattern**: per v0755h + v0734i, when a
  shared-epilogue leaf has multiple variants differing only
  in array constants, each variant is recovered separately
  (the constants are load-bearing and the i960 did not
  abstract them at a higher level).

## What's in this slice (v0769) and what's NOT

- This slice recovers `0x6d1b4` in C with 3 legs verified.
- This slice does NOT recover the `0x6d1b4` callers — the
  function has 0 call xrefs (it is a fall-through target
  reached from other fall-through bodies in the same gap
  run). The test exercises it as a stand-alone.
- This slice does NOT wire `0x6d1b4` into the per-step hook
  table — same deferred slice as v0764..v0768.

## functions.csv

The row does not yet exist in `functions.csv` — v0769 is a
new function-anchor for the gap run `0x6cb0c..0x6dcb8`. A
future slice can add the row at `0x6d1b4..0x6d1ec` with
`recovered-rom-anchor` status and `source = rom-disassembly+unit`
to mirror the v0768 sibling shape.

## Validated

- `git diff --stat src/`: hybrid.c +84 LOC; new test.
- `git diff --stat include/vf2/hybrid/player.h`: +12 LOC
  declaration.
- `git diff --stat CMakeLists.txt`: +5 LOC test entry.
- ctest #61 `vf2_sibling_slot_increment_helper_6d1b4`: PASS.
- ctest #60 `vf2_slot_increment_helper_6d10c`: still PASS.
- ctest #59 `vf2_task_object_cluster`: still PASS.
- ctest #58 `vf2_kill_osage_evaluate_record`: still PASS.
- ctest #57 `vf2_model2a_register_switch_6dccc`: still PASS.
- All wired-hook ctests (cf04, 43888, 439ac, 29598) still PASS.
- vf2_f4_individual_release still PASS in ~17 s.
