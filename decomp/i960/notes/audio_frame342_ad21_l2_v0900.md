# v0900 — Original frame-342 AD 21 00 stream: 12 B/CF commands

Date: 2026-10-20. **Private owner VF2.1 audio ROM executed; no ROM,
snapshot, PCM files or proprietary assets committed.**

## Why the prior command dispatch was insufficient

The fifth real i960 command of frame 342 was `AD 21 00`. Its
original MC68000 dispatcher exits after 248 original interpreted
instructions (PC `0x265C`), creating a stream descriptor,
rather than generating a KEYON itself. The recovered C dispatcher
already matches every meaningful non-stack RAM byte and SCSP
register at that checkpoint (v0899).

The fresh record is stream descriptor 1 at `0x3020`,
status `0x80000001`, next-record pointer `0x0060A6B3`.
The MC68000 ROM-mapped record header at `0x60A6B2` is
`0xCF` and is **not** the `0x8F` AD23/AD24 two-packet grammar.

## Original CPU vector-26 / L2 execution

In the independent owner-ROM test, a new L2 tick was executed
starting from the exact **post-dispatch** audio RAM and SCSP
registers in two distinct runtimes:

- Original MC68000 interpreter: PC `0x1F16` to `0x1F7A`,
  **471 original instructions**, stop status OK.
- Recovered C: `vf2_sound_board_maintain_streams`.

The original emitted **12 command-ring packets**, a single tick,
advancing the write cursor from `0x0014` to `0x0044`
and setting the command count to 12:

```text
B9 2A 00   BA 2A 00   BB 2A 00
BC 2A 00   BD 2A 00   BE 2A 00
B9 07 7F   BA 07 7F   BB 07 7F
BC 07 7F   BD 07 7F   BE 07 7F
```

The original L2 worker writes only 3 bytes of each 4-byte
ring entry; the fourth byte is left untouched. Descriptor #1
retires to `00 FF 00 00` and preserves its pointer `0x60A6B3`.

## Key reverse-engineering discovery: chip versus CPU byte order

The raw chip `epr-17574.30` is word-byte swapped relative
to the memory region assembled by `vf2_romset_build_region`.
For example, bytes at the CPU-accessible `0x60A6B3` read
`B9 2A 00 00`, and the previous byte is the real `CF`
header. The actual end marker is `FF 2F 00`; the following
byte belongs to the *next* record (`CF`), not to this marker.
An earlier strict recognizer incorrectly read chip-oriented
byte order and incorrectly expected a fourth zero terminator
byte, so it failed closed. Both errors were corrected using
the original interpreter observations.

The new `vf2_sound_l2_ad21_expand` recognizes **only**
the original after-header pointer `0x60A6B3`, checks its
header and all 12 validated 4-byte instructions and exact
3-byte terminator, then produces 12 three-byte packets.
It does not admit neighboring stream addresses or unrelated
CF grammars. The L2 C worker preflights alignment, command
counter capacity (at most 243 before adding 12), bounds and
record shape before touching RAM/SCSP. A malformed stream
leaves RAM/SCSP unchanged.

## Owner-ROM exact differential result — PASS

Actual original-ROM owner run with the GitHub Actions
`vf2-m68k-sound-differential-linux` executable from
[CI #2244](https://github.com/celsowm/vf2-decomp/actions/runs/39401761618),
invoked as:

```sh
vf2_m68k_frame342_dispatch_tests /local/owner/vf2-roms --probe-ad21-l2
```

Observed **exit status 0**:

```text
FRAME342_AD21_C68000_PARITY differences=0 first=80000 C_handler=01f02 S_steps=248
FRAME342_AD21_L2_PROBE C_status=0 C_refusal=0 C_refusal_record=255 C_count=12 C_stream_count=0 M_status=0 M_pc=01f7a M_steps=471 M_count=12 M_stream_count=0
FRAME342_AD21_L2_PARITY diff=0 l2_first=80000 scsp_regs_equal=1 c_packets=12 c_cursor=0044
```

**Zero byte differences** across 512 KiB modeled sound RAM
except the original interpreter's stack scratch region
`0x7E00..0x7FFF`; full modeled SCSP register parity.
No additional KEYON is claimed at the L2 expansion stage.
This verifies the newly generated sound-ring entries,
descriptor retirement and timer/control registers **from
the original CPU**, not just an independently fabricated fixture.

The ROM-free `vf2_m68k_ad21_l2_rom_free` regression also
asserts the 12 packets and cursor movement, three-byte
stores preserving the fourth ring byte, descriptor completion,
timer pokes, and seven malformed-record / rollback scenarios.
In [CI #2244](https://github.com/celsowm/vf2-decomp/actions/runs/39401761618)
the GCC suite passed **130/130 tests**.

The stronger original-ROM CTest
`vf2_m68k_frame342_ad21_l2_owner_rom` is only registered
when the owner provides a local `VF2_ROM_DIR`; public CI
never includes proprietary game ROMs.

## Next unresolved step

The 12 newly filed packets are `B9..BE` B-class requests.
A single B-class dispatcher entry, timer progression and
possible voice programming require further **original-MC68000
per-packet** differentials. Do not infer that AD21 itself
produces audible PCM, nor that the observed KICK input caused
a fighter-to-fighter collision. This remains an isolated
original-audio-ROM differential rather than time-accurate
concurrent original dual-CPU scheduling.
