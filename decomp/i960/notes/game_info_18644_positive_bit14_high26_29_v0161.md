# Positive state-8 bit-14 + high-26/high-29 (`0x24004140`)

The positive-threshold `fa_game_info` state-8 matrix now admits the exact fighter flag composition `0x24004140`: state bit 8, bit 6, bit 14 and high bits 26+29.

The user-supplied Virtua Fighter 2 ROM set was used locally as the sequential i960 oracle. The mask was validated across all 12 controlled cases: fighter-0-only, fighter-1-only and bilateral distributions; countdown 0/1; and mode bit 6 clear/set. Every case matched complete snapshot state and instruction/call/return counters.

The second `0x18644` call (`return 0x164c4`) exposes the measured order asymmetry. Its exact counter corrections are kept mask-local rather than generalized to the other high-bit pairs, which remain fail-closed until independently measured. No ROM data or derived snapshots are committed.
