# fa_game_info countdown compare (`0x164f0`)

The recovered task tail already mirrored the two fighter `+0x1200` clears and the `0x0050a0b6` countdown decrement, but it omitted the architectural comparison result of `cmpobe 0,r3` at `0x164f0`. Controlled ROM snapshots show countdown zero exits with `COMPARE_EQUAL` / condition bits `2`; countdown one exits with `COMPARE_LESS` / condition bits `4` after decrementing to zero. Because the source is an unsigned byte, every nonzero countdown uses the latter result.
