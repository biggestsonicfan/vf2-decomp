# v0877 — Optional 68000 engine probe after verified 90 HIGH → author

The default original-ROM 90-to-voice CTest remains bounded and
strict at 0xFAC→0xA8C→0xA92. Both authentic command packets,
9B 42 78 and 9A 01 7F, passed these checks against the user
ROM with the compiled Linux binary.

With an explicit --probe-engine switch, the CTest continues
the original 68000 after 0xA92 to the known 0xC68 playback
engine entry (step budget 50k). It prints status, actual PC,
step count, voice0 status and SCSP slot register status.
This *diagnostic only* does not claim the stepper got to
0xC68 unless the printed result shows that arrival, and does
not change the recovered C semantics or ordinary CTest result.

Reproduce after retrieving the Linux CI binary:

    vf2_m68k_90_to_voice_tests /path/to/vf2 --probe-engine

Proprietary ROM data and traces remain local, never CI inputs.
