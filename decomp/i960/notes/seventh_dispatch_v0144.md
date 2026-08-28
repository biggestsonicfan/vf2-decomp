# Seventh-dispatch differential checkpoint (v0144)

The generic `native-nth-dispatch` continuation was exercised from the validated sixth-dispatch checkpoint against the supported VF2 v2.2 ROM set.

The seventh `fa_game_info` entry at `0x0001645c` / registry `0x00515200` matches the reference i960 state exactly. The sixth-to-seventh continuation adds 36 recovered blocks and 2,170 instructions, with exact CPU, condition state and mutable-memory equality at the target boundary.

Observed cumulative runtime-side counters at the seventh entry are five scheduler entries, eighteen scheduler transitions, five scheduler finishes and ten frame-wait phases.

This does not claim a new gameplay branch. It promotes an already-native continuous corridor to a reproducible ROM-backed checkpoint so subsequent player/gameplay recovery can start beyond the former sixth-dispatch milestone without re-establishing the boundary manually.

CMake now registers `vf2_native_seventh_dispatch` as an optional ROM-backed test through `vf2i960 native-nth-dispatch <rom-dir> 7`.
