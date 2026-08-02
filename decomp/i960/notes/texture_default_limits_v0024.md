# v0.0.24 texture default-limit recovery

"
    "## Scope

"
    "The observed default branch at `0x0004bfe0` is now connected to the
"
    "post-frame hybrid dispatcher. It replaces 22 interpreted i960
"
    "instructions with C, performs the two observed work-RAM writes and
"
    "models the architectural procedure return.

"
    "The accepted branch writes:

"
    "- `0x00003e80` to `0x00550004`;
"
    "- `0x00004e20` to `0x00550008`.

"
    "Alternate mode classes and runtime flag bit 16 remain rejected with
"
    "`VF2_ERROR_UNSUPPORTED`. Unsupported paths do not modify either output.

"
    "## Validation boundary

"
    "ROM-independent tests cover the pure decision, memory writes, rejected
"
    "branches and the complete hybrid CPU post-state, including frame
"
    "restoration, 22 recovered instructions and one procedure return.

"
    "The strict ROM-backed expected totals are updated to 1,268,774 recovered
"
    "instructions and 2,048 interpreted instructions across 144 recovered
"
    "blocks/checkpoints. A supported VF2 2.1 ROM set is still required to run
"
    "`native-second-dispatch` and turn the pending ROM differential claim into
"
    "a locally observed MATCH.
"
    