# VF2 task registry evidence

## Descriptor table

The main program contains 29 contiguous records from `0x00011dc0` through
`0x00012500`. Each record is `0x40` bytes:

```text
+0x00 u32 flags
+0x04 u32 instance (the runtime copies the low byte)
+0x08 u32 runtime-record stride / stack size
+0x0c u32 task entry point
+0x10 u32 address of the global runtime-record pointer
+0x14 u32 scheduler slot
+0x18 char name[0x28]
```

The names begin with `fa_` and include `fa_rob0`, `fa_coli`, `fa_camera`,
`fa_effect`, `fa_sound` and `fa_osage0`.

## Initializer

Function `0x00010cbc` reads the descriptor count from `0x00011d94` and builds
runtime records beginning at `0x00510000`.

For each descriptor it:

1. copies flags, instance, stride and entry point;
2. converts a nonzero scheduler slot to `slot * 25 + 6` and stores it at
   runtime offset `0x38`;
3. writes the runtime-record address through the descriptor's state pointer;
4. advances the runtime cursor by the descriptor stride;
5. clears a 32-byte auxiliary record beginning at `0x0050c000`.

The accepted C implementation is `src/recovered/task_registry.c`. The command
`vf2i960 compare-task-registry` executes the original 647-instruction function
and compares all modeled mutable memory against the C result.
