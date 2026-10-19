# v0867 — Six-word A0 sound record structure and one index alias

The original VF2.1 sound ROM stores two contiguous 128-record families
at 0x60b1e6..0x60bde6, each record 12 bytes / six big-endian words.
This slice reports *structural correlations*, not named audio fields.

## Verified patterns (xx is the embedded one-byte index)

    AD23: 8f bb 21 06 00 9b xx yy 00 ff 2f 00
    AD24: 8f ba 21 07 00 9a xx 7f 00 ff 2f 00

For AD23, yy is 0x78 for indices 0x00..0x43 and 0x7f for
0x44..0x7f. All 128 AD23 record index bytes match their row index.

There is exactly one AD24 *index alias*: the record selected by
external command 0x00AD247C (row index 0x7c, record at
0x60bdb6) contains embedded xx=0x70 instead of 0x7c,
and matches the distinct record at 0x60bd26 for command
0x00AD2470. The original bytes are preserved, not corrected.
Neither AD24 index 0x7c nor 0x70 occurs among the populated
18-by-11 main-data choices; this is a static format detail.

## New functionality

tools/python/audio_a0_rows.py now emits six-word pattern
metadata, counts of embedded-index matches, spans of low-byte
values and any alias exceptions. It does not output a full ROM
record dump or claim MIDI/voice/pitch field semantics.

Three new ROM-free tests cover a complete 256-record fixture,
a deliberately injected index alias, and a tail-byte transition.
The user-ROM regression now requires the actual AD24 alias
and AD23 threshold.

Local: 13/13 test_audio_a0_rows tests passed with the original
user-provided vf2.zip. The unrecovered A0 68000 body after 0x1e98,
SCSP key-on and natural combat SFX remain unproven.
