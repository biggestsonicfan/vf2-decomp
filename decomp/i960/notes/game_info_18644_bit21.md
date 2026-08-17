# fa_game_info `0x18644` bit-21 predicate

The shared tail at `0x18a04..0x18a24` computes `r12 = ((fighter0.flags ^ fighter1.flags) << 15) ^ fighter0.state_flags ^ fighter1.state_flags`. `bbc 21,r12,0x18a24` skips a single `setbit 4,r10,r10`; when bit 21 is set the resulting `r10` bit 4 is stored to `fighter0+0x5b8`.

Controlled forward and reversed fighter probes both execute the path in each of the two swapped `0x18644` calls. Relative to the predicate-clear probe, the full task grows by exactly two instructions, one per helper invocation. The observed post-state changes from `+0x5b8 = 0x2/0x6` to `0x1a/0x16` (forward) and `0x12/0x1e` (reversed), consistent with setting bit 4 while preserving the other accumulated bits.
