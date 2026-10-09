# v0768: 0x6d10c (`slot_increment_helper`) recovered in C

**Slice 4 v4 of `advance_plan_v0764.md`.** A shared-epilogue
leaf from the v0734k-characterised gap run `0x6cb0c..0x6dcb8`,
named in v0734k as one of the small Model 2A memory helpers
in that region.

## Function shape

```
sub_0006d10c: address=0x0006d10c end=0x0006d144 blocks=3 indirect=no
```

56 B in 3 blocks. The leaf:

1. Compute slot index `r7 = max(20, g0 / 1920)`.
2. Read bus pointer from `*(0x50016c)`.
3. Read source slot from `r3 + 0x3520 + r7*4`.
4. Increment by 1 (`r4 = r4 + 1`).
5. Mirror the new value to two slots:
   - `*(0x1d03520 + r7*4)` (extended-bus mirror).
   - `*(0x59c520 + r7*4)` (Model 2A register mirror).
6. Ret at `0x6d144`.

**Important**: the source is **read-only** in this leaf — the
recovery loads from `r3 + 0x3520 + r7*4` but never writes back
to it. Only the two mirror destinations receive the increment.
This is the dual-bus-mirror pattern documented at v0755h for
the v0765 sibling (`sub_0006dccc`).

## Path inventory (3 legs in ctest)

| leg | trigger | r7 index | post-ret state |
|-----|---------|----------|----------------|
| A   | g0 = 1920*100 = 192000 | 100 | dests = source+1 (0x1001); source unchanged |
| B   | g0 = 100 (below 38400)  | 20 (clamped) | dests = 0x421; source unchanged |
| C   | g0 = 5883 (small)       | 20 (clamped) | dests = 0x100; source unchanged |

## Recovery decision points

- **g0 access**: per v0758/v0766 idiom, g0 is
  `cpu->registers[16 + 0]` (i960 global register 0).
- **Two mirrors**: each write goes to both 0x1d03520 and
  0x59c520 (extended-bus + Model 2A pair) — the `vf2_model2a_*`
  API translates both addresses correctly; the bus observer
  does not need to be special-cased here.
- **Source is read-only**: early test versions asserted the
  source was overwritten by the increment; the disassembly
  shows the source load is a `ld` (no corresponding `st`),
  so the source remains at its pre-test value. The ctest
  was corrected to verify the destinations receive the
  increment while the source stays at its prime value.
- **`r7 = max(20, g0/1920)`**: the clamp to 20 means a small
  `g0` skips past the first 20 entries of the slot array.
  This is consistent with the v0734k note "shared epilogue
  cluster" pattern, where multiple parent functions all
  target the same tail to avoid duplicating the slot-update
  bookkeeping.

## What's in this slice (v0768) and what's NOT

- This slice recovers `0x6d10c` in C with 3 legs verified.
- This slice does NOT recover the related Model 2A memory
  writer `sub_0006cc00` (620 B, 2 blocks) which uses this
  leaf as one of its sub-calls. That caller pre-inlines
  this leaf and tail-merges; the leaf is independently
  recovered.
- This slice does NOT wire `0x6d10c` into the per-step hook
  table — same deferred slice as v0764/v0765/v0766/v0767.

## Validated

- `git diff --stat src/`: hybrid.c +99 LOC; new test.
- `git diff --stat include/vf2/hybrid/player.h`: +14 LOC
  declaration.
- `git diff --stat CMakeLists.txt`: +5 LOC test entry.
- ctest #60 `vf2_slot_increment_helper_6d10c`: PASS (0.0 s).
- ctest #59 `vf2_task_object_cluster`: still PASS.
- ctest #58 `vf2_kill_osage_evaluate_record`: still PASS.
- ctest #57 `vf2_model2a_register_switch_6dccc`: still PASS.
- All wired-hook ctests (cf04, 43888, 439ac, 29598) still PASS.
- vf2_f4_individual_release still PASS at ~17 s.
