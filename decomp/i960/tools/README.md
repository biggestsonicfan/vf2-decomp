# `fa_game_info` `0x18644` differential tools

Reusable tooling for the ROM-backed recovery workflow. These scripts replace the temporary inline Python/heredoc staging code used during early exploration.

## One state pair

```bash
python3 decomp/i960/tools/validate_game_info_pair.py \
  ./build/vf2i960 /path/to/vf2-roms 0x102 0x112 --reverse
```

The validator executes the strong chain for every countdown/mode-bit-6 case:

`native child #1 -> caller ROM -> native child #2 -> ROM tail -> 0x10dcc`

It requires both `vf2i960 compare-snapshots` equality and explicit equality of serialized instruction/call/return/interrupt counters.

## Per-call accounting diagnosis

```bash
python3 decomp/i960/tools/diagnose_game_info_pair.py \
  ./build/vf2i960 /path/to/vf2-roms 0x100 0x102 --reverse
```

This runs the first and second `0x18644` calls independently against the same pure-ROM reference and prints architectural equality plus counter deltas for each call site. Use it before changing instruction accounting so a full-chain delta is not incorrectly treated as one global correction.

## Full tracked matrix

```bash
python3 decomp/i960/tools/validate_game_info_matrix.py \
  ./build/vf2i960 /path/to/vf2-roms --workers 2
```

By default this checks rows marked `recovered_exact=yes` in `decomp/i960/game_info_18644_state_vectors.csv`. Add `--all` to attempt all 64 ordered pairs. `--workers` controls ordered-pair parallelism; keep it conservative because each validator launches ptrace and ROM-backed subprocesses.

## Fixture generation

```bash
python3 decomp/i960/tools/make_game_info_fixture.py \
  ./build/vf2i960 /path/to/vf2-roms /tmp/case.vf2snap \
  --state0 0x102 --state1 0x112 --countdown 1 --mode-bit6 0
```

Use `--base existing-164ac.vf2snap` to avoid rebuilding the calibrated caller boundary.

## Native child runner

```bash
python3 decomp/i960/tools/run_game_info_child.py \
  ./build/vf2i960 /path/to/vf2-roms input.vf2snap output.vf2snap \
  --return-address 0x164b0
```

The Linux/x86-64 ptrace harness resolves executable symbols through `nm`; it does not contain build-specific function offsets. A snapshot at `0x164ac`/`0x164c0` is automatically converted to the corresponding native return-IP form before invoking the recovered child.

## Counter comparison

```bash
python3 decomp/i960/tools/compare_snapshot_counters.py \
  expected.vf2snap actual.vf2snap --signature
```

This exists because `compare-snapshots` is primarily a CPU/mutable-memory comparison and must not be used as the sole proof for serialized meta counters.
