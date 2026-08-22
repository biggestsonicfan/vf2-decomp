# `fa_game_info` `0x1645c`: full state-8 bit-14/bit-16 high-26+29 pair

The full dispatcher was measured for field mask `0x64014000`, combining high
bits 26 and 29 with the bit-14/bit-16 composition after the state-8 child
selector/low-field bits are removed. The native dispatcher admission was
checked for fighter-0-only, fighter-1-only and bilateral fighter-record
distributions, countdown `0/1`, mode-byte bit 6 clear/set, and thresholds
`0`, `1` and `2`.

All 36 full-task cases matched the sequential ROM executor at `0x00010dcc`.
The final CPU/memory snapshots matched, and the native recovered instruction
counts matched the reference `run_instructions` values in every case; call and
return counters also matched.

The measured dispatcher accounting is the shared bit-14/bit-16 join rule:
fighter 0 uses `+2*mode6 + countdown - mode6*countdown`, fighter 1 uses
`-4*mode6 - 4*countdown + 4*mode6*countdown`, and the bilateral case uses
`+1 - 2*mode6 - 3*countdown + 3*mode6*countdown`. Unlike the isolated
high-bit compositions, all three distributions leave the final condition
state as `NONE`/`0x3f001000`.

Admission remains limited to the exact field mask and thresholds `0..2`;
other high-bit pairs and thresholds remain ROM-backed boundaries.
