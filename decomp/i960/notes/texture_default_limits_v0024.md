# v0.0.24 texture orchestrator recovery

"
    "## Recovered observed blocks

"
    "The hybrid bridge now replaces these consecutive or surrounding
"
    "texture-orchestrator segments with C:

"
    "- `0x0004bb18`: 21-instruction register-save and body call;
"
    "- `0x0004bcd4`: 5-instruction zero-frame gate and helper call;
"
    "- `0x0004bfe0`: 22-instruction default-limit selection;
"
    "- `0x0004bd24`: four 8-instruction status dispatches;
"
    "- `0x0004bf90`: 11-instruction final status gate and call;
"
    "- `0x0004bfdc`: one-instruction body return;
"
    "- `0x0004bb94`: one-instruction post-body call;
"
    "- `0x0004bc58`: 21-instruction register restore and final return.

"
    "These blocks account for 114 recovered instructions in the observed
"
    "orchestrator path. Unsupported branch classes continue to return
"
    "`VF2_ERROR_UNSUPPORTED` rather than guessing.

"
    "## Expected strict totals

"
    "The ROM-backed validator now expects 1,268,866 recovered bridge
"
    "instructions and 1,956 interpreted instructions across 154 recovered
"
    "blocks and 154 differential memory checkpoints. Recovered procedure
"
    "calls/returns are expected to be 258/300.

"
    "The warning-as-error build and ROM-independent tests validate the C
"
    "semantics and architectural frame transitions. A supported VF2 2.1 ROM
"
    "set is still required to run `native-second-dispatch` and confirm the
"
    "updated headline totals as a full live differential MATCH.
"
    