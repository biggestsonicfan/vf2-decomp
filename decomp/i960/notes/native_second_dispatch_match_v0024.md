# v0.0.24 native second-dispatch ROM match

## Environment

- source: current `master` after texture counter-update recovery;
- ROM: exact supported Virtua Fighter 2 Version 2.1 set, 36 files;
- compiler: GCC 14.2.0;
- configuration: C17 with `VF2_WARNINGS_AS_ERRORS=ON`;
- command: `./build/vf2i960 native-second-dispatch ./roms/vf2`.

The complete build and the five ROM-independent tests passed before this run.

## Result

```text
Native second-dispatch validation: MATCH
Recovered first-sweep task bodies: 7
Recovered first-sweep task instructions: 2808
Recovered scheduler transitions:   6
Recovered transition instructions: 1534
Recovered first-sweep finish:       281 instructions
Inactive descriptors scanned:       1
Recovered traversal instructions:   4623
Post-scheduler bridge instructions: 1270822 total
Recovered bridge instructions:      1268955
Interpreted bridge instructions:    1867
Recovered bridge blocks:            168
Differential memory checkpoints:    168
Recovered bridge calls/returns:     266/300
Frame-wait threshold:               4 visits
Frame interrupts injected:          1 (vector 12)
First geometry instruction:         0x00002eec
First geometry write target:        0x00803008
First changed geometry byte:        0x00803009
Persistent task contexts:           29
Second scheduler entry:             0x00010d54
Second first task:                  fa_game_info
Second task entry:                  0x0001645c
Second registry:                    0x00515200
Snapshot restores after first entry: 0
Final CPU and memory state:         MATCH
```

Observed wall-clock time was 11.62 seconds and maximum resident memory was
127,524 KiB. The process exited with status zero.

## Interpretation

The original i960 interpreter and recovered/hybrid execution reached identical
final CPU and Model 2 memory snapshots. Instruction, call, return and maximum
frame-depth counters also matched at every recovered bridge checkpoint.

The strict totals of 1,268,955 recovered and 1,867 interpreted bridge
instructions are therefore ROM-backed differential results rather than
projections.
