# VF2 snapshot format

Extension: `.vf2snap`

Snapshots store deterministic CPU and mutable machine state. They never store
ROM bytes. All integers are little-endian.

## Version 5 layout

```text
8 bytes       magic: "VF2SNAP\0"
u32           format version (5)
u32           SAT, PRCB, IP, process control, arithmetic control, ICR
u32           compare-result enum and reinitialized flag
u64           executed instructions, procedure calls/returns
u64           interrupt entries/returns
u32           current and maximum local-frame depth
32 × u32      current integer registers
128 × 16 × u32 saved local-register frames
u32           region count (18)
18 × u32      region sizes
bytes         geometry window
bytes         coprocessor port window
bytes         work RAM
bytes         buffer RAM
bytes         video control
bytes         CPU control
bytes         interrupt control
bytes         timers
bytes         tile RAM
bytes         palette RAM
bytes         I/O control
bytes         backup SRAM
bytes         renderer/coprocessor control
bytes         color-translation memory
bytes         texture RAM 0
bytes         texture RAM 1
bytes         polygon luma RAM
bytes         system control
```

Version 5 adds both 2 MiB texture banks so post-frame texture uploads can be
compared and persisted. Version 4 added interrupt counters and corrected the
version 3 duplicate local-frame serialization bug. Older formats are rejected
rather than ambiguously decoded.

`vf2_i960_snapshot_compare` compares architectural CPU state, active local
frames, registers and all 18 regions. Diagnostic counters do not create false
architectural mismatches. `vf2_i960_snapshot_compare_memory` compares only the
mutable regions.

```bash
build/vf2i960 snapshot roms/vf2 out/startup.vf2snap
build/vf2i960 compare-snapshots out/reference.vf2snap out/candidate.vf2snap
```
