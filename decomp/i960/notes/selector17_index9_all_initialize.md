# Selector 17 / bit-7 index 9: ALL INITIALIZE

ROM slot `0x0005fef0` points to entry `0x0005eaa0`. The handler has five states selected by `0x005000a5`: `0x5eac8`, `0x5eb7c`, `0x5eba0`, `0x5ebe8`, and `0x5ecec`.

The selection UI mirrors BACK UP RAM CLEAR, but uses `YES(INIT.)`. Confirmation performs the full factory-style initialization chain: defaults at `0x3320` and `0x3340`, transfer-table rebuild, backup clear/rebuild, CRCs at backup offsets `0x3300`, `0x3302`, and `0x3304`, and the `VIRTUA FIGHTER 2` signature/version at `0x3306..0x3317`. It then displays `COMPLETED`, arms a 100-frame countdown, and returns to TEST MENU index 9.

Native measurements at the `0x0000a6c0 -> 0x0000a010` frame boundary are: state 0 `711/7/8`; state 1 idle `49/4/5`; state 1 select `51/4/5` (`0x1000`) or `48/4/5` (`0x2000`); state 1 cancel `14313/19/20`; state 2 `43/2/3`; state 3 idle `41/2/3`; state 3 return-to-NO `38/2/3`; destructive confirm `54853/30/31`; state 4 countdown `35/2/3`; terminal countdown/return `14308/18/19`. Counts are instructions/calls/returns.

The destructive path is accepted only for the measured RNG seed `0xcbf33340`, matching the strict recovery policy already used by index 7.
