# Opcode-level investigation: reproducible local workflow

> The new `--raw` mode is a static **instruction-byte viewer**.
> It does not execute an opcode or provide an event tracer.

## Show the actual 68000 opcode and all extension bytes

~~~powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
build\Debug\vf2rom.exe verify roms/vf2

# Existing output remains unchanged:
build\Debug\vf2m68k.exe disasm roms/vf2 0x7a6 40

# PC, opcode16, full bytes (up to 12), mnemonic, supported flag:
build\Debug\vf2m68k.exe disasm roms/vf2 0x526 48 --raw
build\Debug\vf2m68k.exe disasm roms/vf2 0x5fe 72 --raw
build\Debug\vf2m68k.exe disasm roms/vf2 0x7a6 64 --raw
build\Debug\vf2m68k.exe disasm roms/vf2 0x902 48 --raw
build\Debug\vf2m68k.exe disasm roms/vf2 0xc68 96 --raw
build\Debug\vf2m68k.exe disasm roms/vf2 0xed8 24 --raw
build\Debug\vf2m68k.exe disasm roms/vf2 0x11d0 64 --raw
~~~

The 16-bit opcode repeats the first two bytes of the
big-endian instruction-byte field. The fixed-width field
retains immediate/displacement/index-extension bytes,
while an unsupported instruction remains marked as such.
These commands are **proposed for local execution**:
no proprietary ROM was available to run them in this
documentation session.

## Correlate with Intel i960

~~~powershell
build\Debug\vf2i960.exe disasm roms/vf2 0x43800 48
build\Debug\vf2i960.exe disasm roms/vf2 0x43888 48

# Requires a valid, never-committed local snapshot at a reachable path:
build\Debug\vf2probe.exe --rom-dir roms/vf2 --snapshot checkpoint.vf2snap --until 0x00043884 --trace --memory-trace
~~~

An arbitrary snapshot may not reach the stop PC. The
i960 drainer was measured to write UART `0x01c80000`,
not `0x009c0000`. Sound L3 files bytes to the 68000 ring
in RAM `0x1000`; cursors/counters live at
`0x2504`/`0x2506`/`0x2508`.

## Dynamic opcode event schema (future observer, **not implemented**)

~~~text
step,cpu,pc_before,opcode16,raw_bytes,mnemonic,operand_size,pc_after,flags,status
event,source_pc,bus_address,width,direction,before,after
~~~

The existing `vf2_m68k_run` stepper has no general
per-instruction observer callback. Its `bus.events`
records selected port pokes, not all RAM/SCSP traffic.
An observer implementation must be **passive**:
compare a ROM-free fixture with and without capture;
PC, registers, flags, RAM, SCSP, retired count,
return and refusal must remain identical.

For each new recovery:

1. Capture one **PRE**-event state. Clone it for C and
   68000 reference stepper; never snapshot after the C call
   for the oracle.
2. Record every executed opcode (not just function entry):
   PC, instruction bytes, D/A registers touched, flags,
   .b/.w/.l, effective address, memory/SCSP writes.
3. Stop on the same boundary **before fetch**, return,
   unsupported opcode or known trap.
4. Compare full modeled RAM, CPU, SCSP, event log and steps;
   identify **the first difference** and don't expand masks
   to hide it.
5. Keep ROM/audio/traces/snapshots local; commit only
   small derived behavioral assertions and code.

Measured checkpoints already in this project:
[8g L3](../../decomp/i960/notes/audio_8g_l3file_v0825.md),
[V1 L1](../../decomp/i960/notes/audio_v1_voice_isr_v0821.md),
[E1c key-on](../../decomp/i960/notes/audio_e1c_recovered_v0825.md),
[E1o allocator refusal](../../decomp/i960/notes/audio_e1o_loopb_wiring_v0825.md),
[8f i960 drainer](../../decomp/i960/notes/audio_8f_drainer_v0825.md).

The highest-return unsolved proof is still a real
gameplay event producing the queue command and audible
voice *in session*, not just synthetic packet staging.
