# v0.0.24 texture orchestrator recovery

## Recovered observed blocks

The hybrid bridge replaces these observed texture-orchestrator segments with C:

- `0x0004bb18`: 21-instruction register-save and body call;
- `0x0004bcd4`: 5-instruction zero-frame gate and helper call;
- `0x0004bfe0`: 22-instruction default-limit selection;
- `0x0004bd24`: four 8-instruction status dispatches and the final 43-instruction inactive scan;
- `0x0004bebc`: four 3-instruction zero-state calls to `0x0004cb64`;
- `0x0004bef4`: four 3-instruction zero-state calls to `0x0004cd18`;
- `0x0004bf2c`: four 2-instruction zero-state loop gates;
- `0x0004bf90`: 11-instruction final status gate and call;
- `0x0004bfdc`: one-instruction body return;
- `0x0004bb94`: one-instruction post-body call;
- `0x0004bb98`: 14-instruction counter update;
- `0x0004bc58`: 21-instruction register restore and final return.

Unsupported branch classes continue to return `VF2_ERROR_UNSUPPORTED` rather than guessing.

## ROM-backed differential result

The exact supported VF2 2.1 ROM set was used with:

```text
vf2i960 native-second-dispatch roms/vf2
```

The command completed successfully and reported:

```text
Native second-dispatch validation: MATCH
Final CPU and memory state:         MATCH
```

Observed execution time was 11.62 seconds. The complete warning-as-error build and all five ROM-independent orchestrator tests also passed before the ROM-backed run.

## Confirmed strict totals

- 1,270,822 bridge instructions in total;
- 1,268,955 recovered instructions;
- 1,867 interpreted instructions;
- 168 recovered blocks and memory checkpoints;
- 266 recovered procedure calls;
- 300 recovered procedure returns;
- one injected frame interrupt;
- 29 persistent task contexts;
- second scheduler entry at `0x00010d54`.

These are now observed differential results, not projected totals.
