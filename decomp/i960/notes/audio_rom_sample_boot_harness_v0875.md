# v0875 — Mandatory sample-ROM regions for real sound-board boot tests

The first attempt to run real VF2.1 ROM-dependent differentials
using the newly published GitHub Actions Linux executables
failed BEFORE any event: sound-board initialization returned
an error. Investigation of src/hardware/sound_board.c shows
the ROM boot's sample uploader accesses the external sample
ROM window. The previous test harness passed NULL sample ROM,
which guaranteed that the boot copy would fail.

Both original-ROM-only CTests now reconstruct and attach
VF2_REGION_SAMPLES in addition to AUDIOCPU (and MAIN_DATA
for the selector test). The sample bank is kept local in memory
and provided to both recovered C sound board and independent
68000 stepper buses. The sample ROM is never embedded in the
repository, distributed in CI or stored in a trace.

This is a HARNESS FIX, not a change to the original game's
audio semantics. The outcome of dynamic 68000 differential
is only proven after the new binaries are downloaded and run
against the user-provided vf2.zip locally.

The previous GCC 121/121 ROM-free suite and 10/10 Python
ROM-backed A0/90 structural tests remain separate evidence.
