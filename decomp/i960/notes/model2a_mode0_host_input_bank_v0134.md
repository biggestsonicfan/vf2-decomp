# Model 2A mode-0 host input bank (v0134)

ROM-backed probing at the recovered `0x00001064` video/input compose boundary showed that the current Virtua Fighter 2 mode (`0x00500f00 == 0`) samples the alternate 315-5649 input bank rather than only the legacy `+0x02/+0x04/+0x06` facade.

Measured neutral bytes are `0xff`, `0x00`, `0x00` at `0x01c00010`, `0x01c00012`, and `0x01c00014`. Clearing individual bits at `+0x10` produces the corresponding low input bits in `0x00500700/0x00500704`, establishing active-low system/coin/start polarity. Setting individual bits at `+0x12` and `+0x14` produces the corresponding P1/P2 byte positions after the compose helper, establishing active-high player polarity for this bank.

The host-input facade therefore mirrors system input at `+0x10` and complements the existing active-low P1/P2 host bytes for `+0x12/+0x14`. The original `+0x02/+0x04/+0x06` path remains intact for the alternate board mode.

With the bridge enabled, holding host COIN+START changes the live game input state from `0x0ff7f700` to `0x0ff7f711`, proving that host input now reaches the recovered game-side input state. No ROM data or snapshots are committed.
