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

## Full-cycle validation and checkpoint state

With the recovered callback branch, the same controlled state crosses the
complete frame/scheduler cycle to the next `fa_game_info` entry at `0x0001645c`
in 36 native differential blocks and 14,290 recovered instructions.

A follow-up two-stage reference experiment initially appeared to disagree by
four bytes at the start of buffer RAM. That was a checkpoint artifact rather
than a runtime mismatch: snapshot version 5 serialized the geometry and video
memory arrays but omitted the Model 2 FIFO's transient
`geometry_write_start`, `geometry_read_start`, `geometry_control` and
`geometry_program_count` fields. Restoring at the intermediate `0x0000a010`
scheduler boundary therefore reset the hidden FIFO state while leaving the
mirrored registers intact.

Snapshot version 6 now serializes and restores those four fields explicitly.
The live differential also compares them as `model2-state`, preventing hidden
hardware-state drift from passing merely because the byte-addressable memory
regions still match. Version-5 checkpoints remain readable: the write/read
pointers and control word are reconstructed from their mirrored registers and
the formerly unserialized program count defaults to zero.

This closes the first branch reached by deliberately leaving the long validated
idle corridor through a live input/control mutation and makes subsequent
checkpoint-based exploration of geometry-active states materially safer.
