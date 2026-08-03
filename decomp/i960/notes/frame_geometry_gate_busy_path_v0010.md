# v0.1.0 0x0000a75c frame_geometry_gate busy path

## Recovered paths

`execute_frame_geometry_gate` previously handled only the clear-flags return at
`0x0000a800` (five instructions). The reference i960 `vf2i960 observe-third-sweep`
command captured four sweep visits and showed the gate's busy subpath firing on
the third scheduler sweep with `flags & 0x04000004 != 0`. The recovered C now
also handles the two observed busy subpaths inside `0x0000a748 -> 0x0000a800`:

- Bit-26 or bit-2 set with `state[0x0050002a] != 17` writes 16 to
  `0x0050002a` and returns through `0x0000a800`. The recovered path covers
  eight i960 instructions and one mutable byte.
- Bit-26 or bit-2 set with `state[0x0050002a] == 17` and the alt byte at
  `0x005000a6` non-zero returns through `0x0000a800` without touching memory.
  The recovered path covers seven i960 instructions.

## Not-recovered subpath

The branch at `0x0000a784` forwards into the unobserved deep reset sequence
when `state[0x0050002a] == 17` and `state[0x005000a6] == 0`. The cluster
- zeroes `0x00500700`, `0x00500704`, `0x00500708` and `0x0050070c`,
- writes `0` to the game-event queue MMIO at `0x00e80004`,
- calls `0x00008ef0` (a fill routine that writes 64 sixteen-bit cells of
  value `0x20` per row for 48 rows of 128-byte stride starting at
  `0x01000000`),
- calls `0x0006116c` (a 16-byte magic write to `0x0059cfe0`), and
- performs an unconditional branch (no `ret`) to the reset entry `0x000000b0`.

Across the four observed sweeps `0x005000a6` is always `0xff`, so this subpath
is unobserved. The recovered gate returns `VF2_ERROR_UNSUPPORTED` for it,
matching project policy for unobserved branches.

## Static analysis of the callees

- `0x00008ef0` is a deterministic stride fill helper. The call site
  `0x00031a30` exists outside the recovered bridge, so the routine is not
  reused on any accepted recovered path; the inline semantics are documented
  here as recovery-aiding evidence but are not exposed as a recovered helper.
- `0x0006116c` has no static xrefs and only writes the 16-byte token
  `0x52455320 / 0x4e4c2053 / 0x4e204544 / 0x20514555` to `0x0059cfe0`. Its
  companion `0x00061198` reads the same token and clears `g0` on a mismatch.

## Validation

A new ROM-independent unit test `test_frame_geometry_gate_busy_paths` in
`tests/analysis/test_orchestrator_bridge.c` exercises:
- the busy-frame-state write 1 -> 16 (8 instructions, 1 byte changed);
- the busy-alt-return with `0x005000a6 = 0xff` (7 instructions, no writes);
- the unobserved deep reset rejection at `0x0000a784` (`VF2_ERROR_UNSUPPORTED`).
All assertions hold and the strict v0.0.24 totals remain unchanged.
