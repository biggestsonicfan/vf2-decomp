# v0767: `task_object` 0x6ca64 + cluster (5 entries) recovered in C

**Slice 4 v3 of `advance_plan_v0764.md`.** All 5 entries of
the task_object cluster (functions.csv lines 96-101) recovered
in a single slice.

## Cluster shape

```
task_object           0x6ca64..0x6ca78   (20 B, 1 block)
task_object_init0     0x6cae0..0x6caf0   (16 B, 1 block)
task_object_init0_ret 0x6caf0..0x6caf4   ( 4 B, 1 block)
task_object_init1     0x6caf4..0x6cb04   (16 B, 1 block)
task_object_init1_ret 0x6cb04..0x6cb08   ( 4 B, 1 block)
task_object_init2     0x6cb08..0x6cb0c   ( 4 B, 1 block)
```

The dispatcher (`task_object`) reads an instance-index byte
from `g13+0x4` and looks up a 4-byte jump table at
`0x6ca78[r3*4]`. The looked-up value becomes the worker's
next-entry pointer at `g13+0xc`. Each init stub then sets
`g13+0xc` to a bare-ret slot or is itself a bare ret.

## Disassembly (per vf2i960 disasm)

```
block_0006ca64 (task_object):
  0006ca64  ldob    0x00000004(g13), r3
  0006ca68  ld      0x0006ca78[r3*4], r5
  0006ca70  st      r5, 0x0000000c(g13)
  0006ca74  ret

block_0006cae0 (task_object_init0):
  0006cae0  lda     0x0006caf0, r15
  0006cae8  st      r15, 0x0000000c(g13)
  0006caec  ret

block_0006caf0 (task_object_init0_ret): ret

block_0006caf4 (task_object_init1):
  0006caf4  lda     0x0006cb04, r15
  0006cafc  st      r15, 0x0000000c(g13)
  0006cb00  ret

block_0006cb04 (task_object_init1_ret): ret

block_0006cb08 (task_object_init2): ret
```

## Path inventory (8 legs in ctest)

| leg | entry | trigger | exp ret | effect on *(g13+0xc) |
|-----|-------|---------|---------|-----------------------|
| A   | 0x6ca64 | index byte = 0 | 0x6ca78 | = init0_ip (0x6cae0) |
| B   | 0x6ca64 | index byte = 1 | 0x6ca78 | = init1_ip (0x6caf4) |
| C   | 0x6ca64 | index byte = 2 | 0x6ca78 | = init2_ip (0x6cb08) |
| D   | 0x6cae0 (init0) | direct | 0x6caf0 | = init0_ret_ip (0x6caf0) |
| E   | 0x6caf4 (init1) | direct | 0x6cb04 | = init1_ret_ip (0x6cb04) |
| F   | 0x6caf0 (init0_ret) | direct | 0x6caf4 | (none, bare ret) |
| G   | 0x6cb04 (init1_ret) | direct | 0x6cb08 | (none, bare ret) |
| H   | 0x6cb08 (init2)     | direct | 0x6cb0c | (none, bare ret) |

The dispatcher needs the ROM-side jump table at `0x6ca78` to
be readable; the test attaches a fake `main_rom` (per the
v0758 pattern) and lays down the three 4-byte table entries.

## Recovery decision points

- **Jump table location**: `0x6ca78` is in program-ROM space
  (the model2a memory map forbids writes here). The test
  attaches a fake `main_rom` and lays the table into it; the
  dispatcher's `vf2_model2a_read_u32` resolves against the
  attached ROM region.
- **Bare-rets de-duped**: `task_object_init0_ret`,
  `task_object_init1_ret`, and `task_object_init2` are all
  functionally identical (4-byte `ret`). The recovery exposes
  a single `hybrid_execute_task_object_bare_ret(machine, cpu,
  expected_ip)` helper that all three wrappers call, with
  each wrapper pinning `expected_ip` to its specific entry
  IP. This keeps the C translation honest (each wrapper
  verifies the entry IP matches) while avoiding duplication.
- **g13 access**: per v0756b / v0758 / v0766 idiom, g13 is
  `cpu->registers[16 + 13] = cpu->registers[29]`. Accessed
  via `VF2_I960_G0_REGISTER + 13u`.

## What's in this slice (v0767) and what's NOT

- This slice recovers **5 entries** of the cluster in C
  with 8 legs verified.
- This slice does NOT wire the cluster into the per-step
  hook table — same deferred slice as v0764/v0765/v0766.
- The cluster has zero call xrefs (`vf2i960 xrefs` reports
  0 references for `0x6ca64`); it's reached via fall-through
  from `object_service_loop` per functions.csv line 102.
  As stand-alone unit tests, the wrappers exercise the
  semantic shape without requiring the live call chain.
- `0x6ca78` (the jump table data) is in program-ROM, so the
  test must attach a fake ROM per the v0758 idiom.

## functions.csv

All 5 rows already exist (lines 96-101) with status
`recovered-observed-branch`. The status remains unchanged
(matching the v0758 sibling pattern). The source column
for these rows could be augmented to `observed-branch+unit`
in a future handoff; that update is not part of this slice
to keep the CSV delta minimal.

## Validated

- `git diff --stat src/`: hybrid.c +206 LOC; new test.
- `git diff --stat include/vf2/hybrid/player.h`: +33 LOC
  declaration.
- `git diff --stat CMakeLists.txt`: +5 LOC test entry.
- ctest #59 `vf2_task_object_cluster`: PASS (8 legs in 0.02 s).
- ctest #58 `vf2_kill_osage_evaluate_record`: still PASS.
- ctest #57 `vf2_model2a_register_switch_6dccc`: still PASS.
- All wired-hook ctests (cf04, 43888, 439ac, 29598) still PASS.
- vf2_native_sixth_dispatch (#124) still PASS at 14.83 s.
- vf2_f4_individual_release (#147) still PASS at 17.57 s.
