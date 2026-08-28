# Native runtime wrapper routing (v0135)

A fresh ROM-backed cold-boot checkpoint was reconstructed at the natural frame-dispatch entry `0x0000a6c0` without the historical active-input mutation used by the selector-17 diagnostic corridor. The natural checkpoint enters frame selector 0.

Before this fix, `vf2i960 native-resume` advanced the selector-0 frame with the legacy `execute_frame_dispatch_tick` shortcut: 392 recovered instructions, one call and two returns. The resulting snapshot diverged from the reference by 3,505 bytes, including the selector mask, countdown and diagnostic text RAM.

The reason was structural. `src/recovered/native_runtime.c` was compiled with a source-wide preprocessor rename `vf2_native_runtime_step=vf2_native_runtime_step_impl`. That correctly renamed the base step definition, but it also renamed the calls inside `vf2_native_runtime_run_until`, causing multi-block execution to bypass the public wrapper chain in `native_runtime_condition.c` / `native_runtime_condition_impl.c`. The already recovered exact selector-0 implementation was therefore unreachable from ordinary `run_until` execution.

This change removes the source-wide rename and explicitly names only the base definition `vf2_native_runtime_step_impl`. Calls made by `vf2_native_runtime_run_until` now resolve to the public wrapped API.

With the fix applied locally against the supplied supported ROM, the same natural selector-0 frame executes 15,853 recovered instructions with 12 calls and 13 returns and matches the reference snapshot exactly at `0x0000a010`. No ROM data or snapshots are committed.
