# v0864b — Invalidate queue provenance on alien and partial writes

The first v0864 observer only handled a complete ring write by the
known 0x43924 filer. That could retain a *stale producer identity*
when another instruction wrote the same slot before the ISR consumed
it. This is a false-positive risk even if the command value happens
to remain the same.

The revised observer invalidates all source attribution for **any
overlapping write**, including a partial/boundary-crossing ring write,
unless the full aligned 32-bit store comes from 0x43924 and is tied
to its exact step. It also invalidates an active transport candidate
when another instruction changes the stage word or cursor. An
unrelated write to another slot does not invalidate a valid producer.

Four new ROM-free tests cover an alien full ring overwrite, a
cross-boundary partial write, an unrelated-slot write (valid
positive control) and an alien stage overwrite. Prior 101 tests
remain green. **105/105 Python tests passed locally** with the
user-supplied ROM in VF2_ROM_ZIP. This validates instrumentation
against synthetic traces and ROM opcode anchors, NOT actual
combat sound. Native C suite and 68000/SCSP differential are
still unexecuted for this slice.

Usage and acceptance criteria remain those of v0864.
