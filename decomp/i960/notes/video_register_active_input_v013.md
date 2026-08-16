# Active-input video-register callback path (v0.1.3)

A controlled continuation from the strict dispatch-256 checkpoint exposes a
previously unrecovered path in the frame interrupt's video-register composer at
`0x00001064`.

The control state at `0x00500700` is normally `0x0ff7f700` in the accepted idle
corridor. Replacing it with `0x00000010` at the dispatch boundary preserves the
same natural CPU/runtime checkpoint but makes the next interrupt produce a
non-zero relevant-change mask. The recovered runtime previously rejected this
state after 22 native blocks at the call from `0x00000c00` into `0x00001064`.

## ROM behavior

The reference i960 reaches `0x00001064` at instruction 14,684,739. The ordinary
idle state returns to `0x00000c04` after 66 instructions. With the controlled
active-input state it returns to the same address after 70 instructions.

The difference is in helper `0x00001200`. Its installed callback is `0x00001284`.
The helper masks the newly enabled controls with `0x00f7f700`, compares that
word with one callback-table byte shifted left by one, and on mismatch executes
the `cmpobne` path that reinstalls `0x00001284`.

For the measured active state the masked value is greater than `0x1fe`, so it
cannot equal an eight-bit table value shifted left by one. The recovered C now
models exactly this proven case and leaves smaller masked values and alternate
callback tables explicitly unsupported pending measurement.

At the `0x00000c04` boundary the reference state is:

- `0x00500700 = 0x0ff7f700`;
- `0x00500704 = 0x0ff7f700`;
- `0x00500708 = 0x00000010`;
- `0x0050070c = 0x00000010`;
- `0x005001dc = 0x00001284`; and
- runtime flags at `0x00508000` remain `0x00008a00`.

This closes the first branch reached by deliberately leaving the long validated
idle corridor through a live input/control mutation. The next differential
boundary is determined by continuing the same controlled state with the rebuilt
native runtime.
