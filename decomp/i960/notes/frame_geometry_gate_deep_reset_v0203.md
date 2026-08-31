# v0203 0x0000a784 deep reset recovery

Measured via synthetic `vf2_i960_run` with ROM-backed machine (same
initialization as `tests/analysis/test_orchestrator_bridge.c`).

## Synthetic state
* `0x00500704 = 0x04000000` (bit26 set to enter busy path)
* `0x0050002a = 17`
* `0x005000a6 = 0` (alt byte clear to force deep reset)
* `0x00500700/0x00500708/0x0050070c` seeded with non-zero to verify zeroing
* `0x00e80004 = 0x12345678`
* `0x0059cfe0 = 0`
* Parent frame at `0x00001000` calling `0x0000a748`

Reference i960 from `0x0000a748` to `0x000000b0`:
```
status ok halt stop address ip 0x000000b0 exec 12566 calls 3 rets 2
0x00500700=0x00000000
0x00500704=0x00000000
0x00500708=0x00000000
0x0050070c=0x00000000
0x00e80004=0x00000000
0x0059cfe0=0x52455320
0x0059cfe4=0x4e4c2053
0x0059cfe8=0x4e204544
0x0059cfec=0x20514555
tile 0x01000000 = 20 00
```

The 48×64 tile fill is `64 * 48 = 3072` cells of `0x0020` at stride
`0x80` (verified at `0x01000000` and `0x010017fe`).

## Recovered C
`src/recovered/texture_bridge_geometry.c:105` now handles `alt_byte==0`:
* zero 4 words `0x00500700/04/08/0c`
* `0x00e80004 = 0`
* inline loop for tile fill (48×64, `0x20`)
* 16-byte magic at `0x0059cfe0`
* `cpu->ip = 0x000000b0`, `executed_instructions += 12566`,
  `procedure_calls +=2`, `procedure_returns +=2`, `bytes_written 6180`,
  `changed_values 6`, `CC = NONE`.

Previously `VF2_ERROR_UNSUPPORTED` (see `frame_geometry_gate_busy_path_v0010.md`);
now strict differential via synthetic harness and ROM-independent test
`test_frame_geometry_gate_busy_paths` (updated in
`tests/analysis/test_orchestrator_bridge.c:2100`) which now checks the
deep reset path succeeds and validates zeroed state + magic + tile.

## Validation
* `ctest -R orchestrator_bridge -V` passes
* Full `ctest -C Debug -j4` 55/55 pass
* No change to other recovered totals
