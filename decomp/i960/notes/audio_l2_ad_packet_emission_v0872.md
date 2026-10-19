# v0872 — Recover the A0 AD-family L2 B/90 packet expander

Original VF2.1 Motorola 68000 from an A0 descriptor initialized
with selected 12-byte record pointer+1, countdown word 1:
- Worker 0x1f7c..0x1f9c decrements the countdown to 0, fetches
  first stream byte (AD23: BB; AD24: BA).
- The jump at 0x1fba selects the B handler 0x1ffc. The B handler
  writes byte0 and next record byte to ring, skips its 0x10 arm for
  value 0x21, reads 0x06/0x07 and writes packet byte2.
- 0x211e/0x2128 advances the write cursor by 4 and count by 1.
- A literal 00 means continue; the next negative opcode 9B/9A
  selects the default 90-class handler 0x2080. It emits 9B/9A,
  the embedded record index (including AD24 0x7C -> 0x70), and
  the record tail 78/7F as another 3-byte ring packet.
- FF 2F terminator reaches 0x215a. With descriptor bit3 clear,
  0x2192 clears descriptor status and 0x2198 is the RTS.
- Per original instruction shape, fourth byte of each 4-byte ring
  slot is NOT written by these appliers.

Concrete original-ROM examples:
  00AD2342 -> BB 21 06 | 9B 42 78
  00AD2401 -> BA 21 07 | 9A 01 7F

New src/hardware/sound_l2_ad.h recognises the complete original
12-byte record families with strict pointer bank/stride and tail
guards, without storing any original ROM material in the project.
256 candidate records were checked against the local user ROM and
all conformed; the standalone native test covers every embedded
index in both families, mutation refusal and AD24 alias.

The L2 tick implementation preflights the live one-countdown
record and ring cursor/count capacity, still refusing unknown
worker paths (unchanged state). When admitted, it writes both
packets to the sound ring, updates count and write cursor, clears
the worker descriptor, then applies the same L2 timer pokes.

Validation:
- Native C shape test: GCC and Clang -std=c11 -Wall -Wextra
  -Werror -pedantic pass locally.
- 256 actual original-ROM records were structurally checked.
- The existing L3->A0->L2 CTest now steps the **original Motorola
  68000** from 0x1f7c through 0x2198 and compares both packet slots,
  count/cursor, and the 32-byte worker descriptor to recovered C.
  This ROM-dependent executable differential requires the owner's
  CMake/ROM build and has not been executed in this container.
  CI compiler green is not a substitute for a passing differential.

This recovers sound command emission for two original data families,
NOT necessarily audible gameplay. The second 90-class commands
still need dispatch to a voice and original SCSP key-on measurement.
