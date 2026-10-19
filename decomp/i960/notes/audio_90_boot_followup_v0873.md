# v0873 — VF2.1 AD23/AD24 follow-up 90 HIGH routes using original boot descriptors

The B/90 sound-stream emission recovered in v0872 produces
9B 42 78 and 9A 01 7F as the second three-byte packet from
real A0 sound command choices. Instead of fabricating a live
voice, follow the exact boot-descriptor images.

The original 68000 boot (0x336) loads 2 × 10 descriptors
from addresses pointed to by 0x603066 and 0x6030a6.
All boot rows are reconstructed, including scaling of
descriptor byte 10 based on byte 3. The original ROM has
unique active matches:

| packet | descriptor | mode field | ROM[0x60c6a6+byte2] | HIGH table entry | word / alt | Loop-B 0x1515 |
| --- | --- | --- | --- | --- | --- | --- |
| 9B 42 78 | #11, channel 0B | FF | FD | 60488A | 0242 / E000 | D6 |
| 9A 01 7F | #10, channel 0A | FE | FF | 6045E4 | 0261 / E000 | 81 |

The descriptor high-table row pointers are 0x6046FE for
9B and 0x6045DE for 9A. Both six-byte entries have
non-sentinel first words; the ROM's conditional 90 HIGH
branch therefore proceeds to Loop-B at 0x902.

This is ROM data + recovered branching, not a demonstration of
a complete 68000 dynamic path or SCSP key-on.

Added:
- tools/python/audio_90_boot_route.py: ROM-side descriptor
  loading and bounded high-table / Loop-B-byte verification.
- tools/python/test_audio_90_boot_route.py: eight tests, including
  optional local original-ROM golden checks and malformed pointer,
  disabled/duplicate channel, wrong class and sentinel refusal.
- tests/m68k/test_90_ad_followup.c and registered CTest:
  linked sound_board.c integration with exact boot-shaped 90
  packets and synthetic ROM fields; verifies word/mode/current
  and Loop-B lookup, original allocator 0xFAC boundary; tests
  sentinel consume and invalid HIGH pointer refusal.

Local: 8/8 Python tests including supplied VF2.zip original ROM.
C test is compiled/run in normal GitHub CI. Native 68000
stepper comparison for this 90 follow-up and actual SCSP
key-on remain separately required; no full ROM bytes or
audio samples are committed.
