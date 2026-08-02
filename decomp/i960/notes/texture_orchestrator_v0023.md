# v0.0.23 texture orchestrator profile

This is a pure-evidence release. v0.0.23 records observations of the texture
orchestrator cluster without claiming recovery of any block in it. No new
`execute_*` function was added to `src/recovered/texture_bridge.c`, no new
`vf2_hybrid_bridge_kind` was defined, and no `case` was added to
`vf2_hybrid_post_frame_bridge_execute`. The headline numbers below are
unchanged from v0.0.22.

## Cluster under observation

The cluster spans the inclusive address range
`[0x0004bb18, 0x0004c180]` in maincpu. The three target addresses named in
`docs/ROADMAP.md` §v0.0.23 are:

- `0x0004bb18` -- top-level texture orchestrator (driver/dispatcher);
- `0x0004bcd4` -- helper adjacent to the orchestrator;
- `0x0004c180` -- helper adjacent to the orchestrator.

These three addresses are geographically bracketed above by the recovered
texture-decode family (`0x0004c3f0` symbol-table build, `0x0004c4d4` pair-table
build) and below by the recovered parameterized decoders
(`0x0004c6e0` byte-decode, `0x0004c868` byte-run, `0x0004c928` tree expand,
`0x0004cc28` word-decode, `0x0004cce8` word-run, `0x0004ce88` color convert).
The diagnostic helpers recovered in v0.0.22 (`0x00007fc0`, `0x0004f944`,
`0x0004d16c`, `0x00009444`, `0x0004d2c0`) and the v0.0.22 gameplay helpers
(`0x0000281c`, `0x000026ec`) are reachable children of this orchestrator.

## Instrumentation

A new read-only developer command was added:

```bash
build/vf2i960 trace-orchestrator /path/to/vf2
build/vf2i960 trace-orchestrator /path/to/vf2 out.csv
```

Behavior:

- reuses the existing `command_native_dispatch(rom_directory, true)` loop;
- on every interpreted native step whose `ip_before` is in
  `[0x0004bb18, 0x0004c180]`, emits one CSV row to the output file;
- aborts (writes no usable evidence) if the strict total assertions at
  `tools/vf2i960/main.c:4305-4322` do not all hold.

Because the command only writes records after the existing MATCH gate has
passed, it is provably non-behavior-changing. The 21 CTest targets that
exercise `command_native_dispatch` continue to invoke it with
`g_orchestrator_trace_file == NULL` and therefore behave identically to
v0.0.22.

The CSV columns are:

```text
step,ip_before,ip_after,frame_depth,arithmetic_control,
executed_instructions,procedure_calls,procedure_returns,
maximum_local_frame_depth,instruction
```

The default output path is `decomp/i960/notes/texture_orchestrator_v0023.csv`
(the present directory). The CSV is treated as recorded evidence, never as a
configuration source -- no source file reads it.

## Claim boundary

Every observation in the CSV is labelled `evidence: verified` because each
value is read directly from `native_cpu`, `native_machine`, or the decoded
`vf2_i960_instruction`. Any *interpretation* of those values (e.g. assigning a
role such as "driver" to `0x0004bb18`) is labelled `evidence: medium` per
`docs/DECOMP_GUIDE.md` and is not used by any code path.

The role assignments stated for the three target addresses above are
inferences from spatial layout and the recovered-children inventory; they are
**not** proven by a live invocation sequence in this release.

## Differential totals (unchanged from v0.0.22)

```text
bridge instructions:          1270822
recovered instructions:       1268752
interpreted instructions:        2070
recovered blocks/checkpoints:  143/143
recovered calls/returns:       250/297
final state:                   MATCH
```

The strict-equality assertions at `tools/vf2i960/main.c:4305-4322` were not
modified. The release literally cannot ship if any of these numbers move.

## Hand-off to v0.0.24

v0.0.24 opens by reading this CSV (or by re-running `trace-orchestrator` on a
fresh ROM set) and selecting the first orchestrator address for which *all* of
the following are observed for at least one live invocation:

1. entry-state register snapshot (g0..g15, frame pointer, instruction pointer);
2. complete downstream call graph listing which `vf2_hybrid_bridge_kind`
   children fire and in what order;
3. memory post-state diff against the original-machine (interpreted) baseline
   via `compare_hybrid_snapshots`;
4. native vs. original architectural counter parity
   (`executed_instructions`, `procedure_calls`, `procedure_returns`,
   `maximum_local_frame_depth`).

The first address satisfying those four predicates is the candidate for
v0.0.24 recovery, to be written following the v0.0.22 template
(`texture_bridge.c:359-415` and siblings). Any branch in the candidate that is
not exercised by an observed invocation continues to return
`VF2_ERROR_UNSUPPORTED` per `docs/DECOMP_GUIDE.md` step 5/8.

The `0x00001f5c` geometry-preparation cluster mentioned in ROADMAP §v0.0.23 is
deferred. It is geographically and semantically separate from the texture
orchestrator and would dilute this release's evidence collection.
