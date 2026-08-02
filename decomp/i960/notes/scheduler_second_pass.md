# Scheduler second-pass evidence

## Proven path

- final first-pass task return checkpoint: `0x00010dcc`;
- recovered scheduler finish continuation: `0x0000a014`;
- final inactive descriptor: index 28, registry `0x00516400`;
- descriptor name observed: `fa_pol_test`;
- recovered finish cost: 281 instructions;
- post-frame bridge: 1,270,822 total instructions, 1,268,752 recovered and 2,070 interpreted;
- frame wait released by vector 12 after four visits to `0x00010f98`;
- second scheduler call site: `0x0000a010`;
- second scheduler entry: `0x00010d54`, now recovered in C;
- descriptors scanned on re-entry: 14, of which 13 are inactive;
- second first-task entry: `fa_game_info` at `0x0001645c`;
- registry on entry: `0x00515200`;
- no snapshot restore after the initial first-task fixture.

## Hardware corrections required

Architectural reset reads the interrupt stack at `PRCB + 24`, yielding
`FP=0x005ff500` and `SP=0x005ff540` for the supported program. The bridge also
writes texture RAM through mirrored address `0x12600600`, requiring the two
Model 2A texture banks and their mirrors to be part of the validation model and
snapshot format.

## Claim boundary

The first sweep, including its end-of-list return, is recovered C. The
post-frame bridge is partially recovered. v0.0.22 proves persistent continuity and
a recovered-C second-pass entry; it does not claim a native frame loop.


v0.0.22 replaces 1,268,752 of the 1,270,822 bridge instructions with bounded texture, palette, diagnostic, geometry-register and scheduler-entry C blocks; 2,070 remain interpreted.
