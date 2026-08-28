# Warm display/profile continuation recovery (v0139)

The warm post-boot corridor following `0x000098cc` was revalidated against the supported VF2 Version 2.1 ROM as the project moved from service/test exit toward normal runtime initialization.

This recovery admits the ROM-observed warm call depths while retaining the historical synthetic depths as separate fail-closed cases. It covers video/display constants, task-registry and graphics-buffer setup, render/game defaults, object/effect/input tables, I/O initialization, game-data copy, display offset and frame accumulator state, profile defaults, gameplay globals, display-profile setup, float/input profile loading, palette ramp/build, and the nested display-runtime helper up to `0x00009a00`.

The important algorithmic correction is the palette builder at `0x00002c38`. ROM disassembly shows the inner `cmpdeco` loop starts from 47 and therefore executes 47 columns, while the outer `cmpinco` loop executes 27 rows. The previous 28-by-32 model was incorrect. The recovered 27-by-47 builder accounts for 39,208 instructions and preserves the ROM-observed epilogue state.

The warm profile-default path at `0x000113f4` reproduces the signed byte override conversion to float, multiplication by 0.5, selector-profile comparison, and the two halfword override comparisons. The measured warm case uses override 15, which becomes 7.5 and equals the selector-1 profile value.

The nested display-runtime helper also reproduces the ROM `stt r8` triple-store through `stream + 0x5c` and preserves the measured state/stream registers. The Model 2A I/O model treats the observed `0x4e` write to mode-0 `+0x10` as a 315-5649 command/configuration write rather than overwriting the readable sampled system-input backing byte.

The recovered corridor previously reached `0x00009a00` with an exact ROM snapshot match. No ROM bytes or snapshots are committed.
