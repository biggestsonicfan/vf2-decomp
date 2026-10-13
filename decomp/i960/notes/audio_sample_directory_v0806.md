# v0806 - sample directory located in the 68000 program (measured)

Evidence note, no code change.

## Result

`epr-17574.30` (word-swapped relative to the 68000's view; ROM at `0x600000`)
holds a sample directory of **650 entries x 20 bytes**, big-endian, at program
offset `0x5b28`:

| offset | field |
|---|---|
| +0x00 | start address (`0x800000` + offset in the sample ROM) |
| +0x04 | length in bytes |
| +0x08 | loop start address (= start when there is no loop) |
| +0x0c | loop length in bytes (0 = no loop); loop start + loop length = end of sample, to 2 bytes |
| +0x10 | envelope word, copied by the note-on path into the slot's EG registers (`move.l $10(a2,d3),$08(a5,d4)`); byte +0x13 & 0x1f indexes a rate table |

The note-on path reads it at `0x0c1e..0x0c2e` (`lea $5000,a2`, index
`(word & 0x7ff) * 0x14`); the 68000 mirrors the table to sound RAM `0x5000` at
boot.

## Evidence

- MAME (`vf2`, audio CPU address space, frame 700): sound RAM `0x5000..` equals
  the ROM table for the first 131 entries. Entries 131-133 and 144 differ at
  run time (start address OR 0x80000000 style values), the rest of the table
  (to entry 649) equals the ROM.
- 402 entries have a non-zero start; entries 167 and 511 have length 0
  (reserved slots), leaving 400 usable; 57 loop. All ranges lie inside the
  8 MiB sample ROM.
- Commands are MIDI-style bytes (`0x80` off, `0x90` on, `0xA0`, `0xB0`, `0xC0`
  program change, `0xE0` bend); a program selects a 6-byte-per-note table via
  the pointer tables at `0x6032c6` and `0x6053aa`; word 0 of a note entry is
  the sample index (`0xff` = none).
- In MAME's attract mode the 68000 receives no sound command in ~3 minutes of
  emulation (only the boot SCSP initialisation), so no key-on was captured.

## Not done

Pitch (octave/fine tune from note and tuning word, tables near `0x60c3e6` and
`0x60c6a6`), volume and envelope mapping. Interaction with the corrected
PC-relative base (PR #123) is unaffected: the directory is plain data.
