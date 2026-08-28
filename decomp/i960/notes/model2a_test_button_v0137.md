# Model 2A TEST button input (v0137)

Fresh ROM-backed validation of the natural cold-boot TEST MODE exit path showed that the board's TEST switch is distinct from the SERVICE switch. The game observes TEST as active-low bit `0x04` in the mode-0 system input byte read at `0x01c00010`, while SERVICE remains active-low bit `0x08`.

The platform input API now exposes a dedicated `VF2_PLATFORM_BUTTON_TEST` bit and the Model 2A system-port bridge maps it to active-low `0x04` without changing the existing COIN, START or SERVICE mappings.

Using the supplied supported ROM, asserting TEST while TEST MODE is positioned on `EXIT TEST MODE` changes the live menu state from `0x0000000b` to `0x0000008b`, arms the observed 320-count exit delay, counts it down to zero and enters the ROM's warm-boot path without modifying the frame selector or other game state directly.

No ROM data or snapshots are committed.
