# Selector-3 phase-13 ROM validation (v0.0.27)

The phase-13 random fighter-selection correction was independently rechecked against the supported Virtua Fighter 2 Version 2.1 main-CPU ROM set.

The four i960 program EPRs used for this check match the repository manifest exactly:

```text
epr-18385.12  388bb5cd0304e6a9656b3ac12aa1a3e8c824edd96686479131ec341149ae4cb0
epr-18386.13  07bede90efaf3374c37bb95e6914ae47e3ca7a95e6f65385453b3940564bff3b
epr-18387.14  3eb7acd0ecc0dfdbc92d48642ac1aecd70da41c3a59bb654c7c99f2c369bc4a3
epr-18388.15  e2ee070a472ae7c8adc41740e5b2448b38b15373d09ca1bf497944061a663578
```

After reconstructing the `maincpu` region with the manifest's `load32_word` layout, the bytes at the phase-13 selection block are:

```text
0000bdc8: 08 d7 ff 09 0b 0c 74 70 f8 bf 4b 32 fc d6 ff 09
0000bdd8: 0b 0c 7c 70 f8 ff 4b 32 08 c0 73 35 0d c8 7b 59
0000bde8: 00 30 20 90 04 08 50 00 b0 21 71 82 00 30 20 90
0000bdf8: 08 08 50 00 b0 21 79 82 00 30 b8 90 04 08 50 00
```

These bytes are the same block documented in `selector3_phase13_measurement_v0026.md`: two calls to the RNG helper, modulo-11 reduction, retry on remainder 9, equality-sensitive `+13` adjustment of the second fighter, and byte stores through the two live fighter-task pointers.

The temporary GitHub Actions dump workflow used during the correction is no longer needed and was removed after this validation. The next useful recovery step is to obtain a full live-state differential checkpoint for phase 13 composed with the shared phase-7 setup, then continue from the first downstream mismatch rather than reopening the already-settled random-selection instructions.
