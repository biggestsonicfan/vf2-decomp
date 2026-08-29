# Warm selector handoff fidelity (v0142)

The warm post-boot final cluster reaches the frame dispatcher with a non-zero local-frame depth. The generic bridge used to overwrite `r13` with synthetic `(start_depth << 8) | final_depth` bookkeeping, producing `0x303` at the measured warm boundary while the ROM preserves the architectural value `0x003`.

The condition-specific selector wrappers already limited this bookkeeping to synthetic depth-zero runs. This change applies the same rule to the shared bridge: depth-zero diagnostics keep the marker, while warm execution preserves the ROM register value.

The warm selector-0 signature fast path has already been ROM-validated to advance selector 0 to selector 2 with 34 instructions, two procedure calls, and three returns. No ROM bytes or snapshots are committed.
