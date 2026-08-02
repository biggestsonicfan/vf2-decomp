# Architecture

The final target is a **native, non-matching C port**. Decoding, abstract
interpretation and instruction execution are recovery tools, not final runtime
components.

## Recovery pipeline

```text
User-supplied ROMs
        |
        v
Validation and deterministic region reconstruction
        |
        +---------------------------+
        |                           |
        v                           v
Static i960 analysis          Semantic i960 executor
CFG, xrefs, values            traces and snapshots
        |                           |
        +-------------+-------------+
                      |
                      v
            Differential validation
          original semantics == recovered C
                      |
                      v
              Accepted recovered C
                      |
                      v
       Portable Model 2 compatibility APIs
```

## Boundaries

- `src/i960/decoder.c` describes instruction encoding.
- `src/i960/executor.c` provides temporary deterministic semantics, including
  local-register procedure frames.
- `src/i960/snapshot.c` serializes mutable evidence without ROM data.
- `src/analysis` creates static evidence and disposable pseudocode.
- `src/recovered` contains manually accepted semantic C.
- `src/hardware/model2a.c` is a bounded validation model, not a general Model 2
  emulator.

## Current accepted recovery boundary

```text
0x000000b0 -> 0x000001b0  startup stage 1, matching C
0x000001b0 -> 0x0000052c  board initialization, matching C
0x00010cbc                 task-registry initializer, matching C
0x00000d50                 timer interrupt handler, matching C
0x00010d54                 scheduler registry consumer identified
0x0001645c -> 0x00010dcc  seven first-dispatch tasks/transitions in C
0x00010dcc -> 0x0000a014  first-sweep finish in C
0x00010d54                 second scheduler traversal reached
0x0001645c                 second fa_game_info entry validated
```

The i960 reset path reads the initial stack from `PRCB + 24`. The bounded Model
2A model includes texture banks at `0x12000000` and `0x12400000`, each 2 MiB
and mirrored into the following 2 MiB window. These additions allow the
post-frame bridge to execute deterministically through the second traversal.
The bridge itself is still interpreted and is not accepted recovered C.

## Rules

1. Unsupported instruction and MMIO behavior must fail explicitly.
2. Generated pseudocode is never accepted source by itself.
3. A readable translation is not evidence of correctness.
4. Recovered behavior should have a deterministic checkpoint or trace.
5. ROMs, extracted assets, full traces and snapshots are never committed.
6. The final game loop must not contain an i960 dispatcher.
