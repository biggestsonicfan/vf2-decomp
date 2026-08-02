# Decompilation workflow

## Static evidence

```sh
build/vf2i960 analyze roms/vf2 out/analysis
```

Inspect the function list, CFG, xrefs, abstract values, indirect targets and
pseudocode before assigning names or writing recovered C.

## Dynamic evidence

Execute a bounded path:

```sh
build/vf2i960 execute roms/vf2 0x000001b0 2000000
```

Capture local evidence:

```sh
build/vf2i960 trace roms/vf2 out/path.csv 10000
build/vf2i960 snapshot roms/vf2 out/path.vf2snap
```

## Function lifecycle

1. Identify a candidate using static analysis.
2. Record direct calls, strings, hardware addresses and structure offsets.
3. Create a deterministic entry-state checkpoint.
4. Extend the executor only when verified instruction semantics are missing.
5. Write the smallest semantically equivalent C function.
6. Run the original path and the recovered C path from equivalent state.
7. Compare modified memory, registers, return values and hardware commands.
8. Add a ROM-independent unit test for the recovered behavior.
9. Move the function into `src/recovered` only after validation.
10. Update the symbol/evidence CSV and notes.

## Evidence levels

- **verified:** directly encoded by vectors/tables or reproduced by comparison;
- **high:** independent static and dynamic evidence agree;
- **medium:** strong static-analysis hypothesis;
- **low:** provisional organizational name.

## Generated pseudocode policy

Files in `out/analysis/pseudo-c` are disposable navigation aids. They may
contain register variables, gotos, unresolved calls and placeholder memory
operations. Never copy a generated function wholesale into `src/recovered` and
label it recovered.

## Current limitations

- execution coverage is intentionally limited to verified startup semantics;
- static propagation remains mainly intraprocedural;
- computed continuations depending on caller state can remain unresolved;
- no scheduler, game task, TGP or sound program has been recovered yet;
- the Model 2 memory model contains only regions required by current paths.

## Files that must not be committed

- ROM archives or reconstructed ROM regions;
- extracted textures, models, samples or substantial strings;
- full traces containing proprietary behavior data;
- `.vf2snap` state files;
- generated pseudocode copied from ROM analysis.
