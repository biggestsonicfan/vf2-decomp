# v0309: 0x19ef8 live-drive blocker + frontier call-edge ranking

## Verdict

1. **Measurement-only.** A live hybrid drive of `player_flags` bit 5
   from `out/punch10.vf2snap` fails closed with `unsupported
   operation` after 28 scheduler entries, at snapshot IP `0x16464`,
   **before the first compared block**. The `0x19ef8` sibling still
   has no reference witness.
2. `tools/python/frontier.py` gains `rank_call_edges`: call/bal edges
   with source/target function attribution and a boundary-crossing
   flag, printed after the call-target IP histogram.

## vf2cycles sidecar trap

`vf2cycles` loads `<snapshot>.runtime` next to the `.vf2snap`. Copying
only the snapshot produces `I/O error` even when the bytes are
identical to a working park. Copy or regenerate the sidecar:

```text
punch10.vf2snap
punch10.vf2snap.runtime
```

`vf2probe --output-snapshot` does **not** write the sidecar.

## Live bit-5 drive

```text
# copy both files
vf2probe --snapshot out/punch10.vf2snap \
  --set-u32 0x00510980=0x20 --max-steps 1 \
  --output-snapshot out/v308/punch-b5.vf2snap
# punch-b5.vf2snap.runtime copied from punch10

vf2cycles --rom-dir roms/vf2 \
  --snapshot out/v308/punch-b5.vf2snap --input 16 --cycles 1
# Endurance stopped: unsupported operation
# Snapshot repeated address: 0x00016464
# Scheduler entries/transitions/end: 28/45/14
# Compared blocks: 0
```

Reference-only `vf2probe` from the same park still spins in the
`0x10fa0` wait loop (no IRQ injection). `--raise-irq` does not break
that loop from this park; `--enter-interrupt` was not validated here.

## Frontier call edges

```text
frontier.py traces.jsonl --limit 40
# ...
# call edges (source -> target):
#   0x000164ac->0x00018644  x2  caller -> callee(candidate) *
```

Unit test: `test_rank_call_edges_crosses_boundary`.

## Proof

- `python tools/python/test_frontier.py` — all passed (duckdb skip)
- No C recovery; `0x19ef8` bit-5/6/21/23 guards stay fail-closed
- PUNCH / input-17 / ctest unchanged (no runtime edit)
