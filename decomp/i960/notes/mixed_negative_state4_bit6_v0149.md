# Mixed negative state-4 bit-6 recovery (v0149)

The mixed negative-threshold state-4 family with bit 6 in `fighter + 0x1a4` is now admitted to the recovered `0x00018644` C path when the global countdown byte at `0x0050a0b6` is zero.

ROM-backed differential coverage used twelve counterpart states (`0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 15, 255`), both state-4 orientations, three bit-6 distributions (state-4 fighter only, counterpart only, bilateral), and both mode-bit-6 settings. All 144 cases matched the reference i960 CPU state, mutable memory, and accounting at the `0x00010dcc` task boundary.

Countdown-nonzero cases remain outside this admission because the current whole-task differential probe reaches the intermediate `0x000164f8` boundary before the native recovered block can be compared. They stay fail-closed/interpreted until separately measured.
