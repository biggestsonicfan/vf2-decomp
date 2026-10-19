# v0866b — Two adjacent 128-by-12-byte A0 record banks

VF2.1 sound-ROM pointer directory at 0x608e12:

- Family AD23: row 0x60959c, max selector 0x7f, first record 0x60b1e6.
- Family AD24: row 0x60979e, max selector 0x7f, first record 0x60b7e6.
- Every family contains 128 ordered pointers, all spaced 12 bytes apart.
- The 256 records are contiguous through end-exclusive 0x60bde6
  (3072 bytes total). The first family's last record ends exactly
  where the second family begins.

This is sound-ROM STRUCTURE, not proof of original gameplay execution,
field semantics, MIDI/SCSP behavior or audibility.

audio_a0_rows.py now verifies all 256 pointer records and refuses
bad max, non-12 stride, odd/out-of-bank address or gap between banks.
The optional user-ROM golden test pins both bounds. Two ROM-free
regression cases cover contiguous records and one deliberately
corrupted stride. The fixture places its pointer rows apart to
avoid artificial overlap.

Local validation: 10/10 tests for this new module with the
user-supplied VF2.zip. The separate CTest A0 stepper stop remains
unrun with the full local C project; successful CI compilation is
not proof that this ROM-backed runtime differential passes.
