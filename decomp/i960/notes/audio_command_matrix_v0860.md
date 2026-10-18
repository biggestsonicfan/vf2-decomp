# v0860 — ROM-verified 18×11 sound-command selection matrix

This is a **data-structure recovery**, not an audio gameplay admission.
The Model 2A i960 sees `VF2_MAIN_DATA_BASE = 0x02000000`
(`include/vf2/model2a.h`). The MAME manifest file region allocates
`0x02400000` bytes, but that number is a **size**, NOT the i960
base address. This distinction matters for interpreting ROM pointers.

## Source-backed layout

- Address `0x0201e44c` (primary main_data offset `0x1e44c`):
  null entry for profile **0**, then 18 pointers for profiles **1..18**.
- Pointer records: 11 little-endian u32 command slots per profile
  (44 bytes each). The first populated record is at `0x0201e4e4`.
- The two first-region MAME `LOAD32_WORD` EPROMs are
  `mpr-17560.10` and `mpr-17561.11` (2 MiB each); only these
  are needed to rebuild the local first 4 MiB of main_data.
- 198 total cells, **107 filled**, 91 zero/empty, **98 distinct
  commands**, with **7 distinct command IDs used at multiple cells**.
  Families: 64 entries `0x00ad23xx` and 43 `0x00ad24xx`.
- Four profile records are entirely empty. Neither profile index
  nor column index has been semantically named as a particular move
  without a natural gameplay trace.

The table maps a **choice** to the command word the original ROM stores.
That is a useful recovered fact, but it does not prove that the game
takes the path, that a command reaches `0x43888`, or that the 68000
keys a voice.

## Implementation

`tools/python/audio_command_matrix.py` reconstructs only the relevant
ROM pair, validates the null profile and every pointer/command family,
and supports forward lookup and reverse mapping without generating
or committing any proprietary table dump.

~~~sh
python tools/python/audio_command_matrix.py roms/vf2 --lookup 9 0
python tools/python/audio_command_matrix.py roms/vf2 --reverse 0x00ad2342
python tools/python/audio_command_matrix.py /path/to/vf2.zip --json

# ROM-free cases plus a ROM-backed golden test:
VF2_ROM_ZIP=/path/to/vf2.zip python -m unittest discover -s tools/python -p 'test_audio_*.py'
~~~

The local user's VF2 zip produced 107 / 91 / 98 and family counts
64 / 43; the combined Python audio-tool suite passed **50/50**
(including the ROM-backed golden test). Full C tests and a naturally
emitted gameplay sound have **not** been rerun or established in this
slice. The tool does not alter runtime, queue or SCSP.

## Next proof

Correlate a real executed selection (profile and column registers at
`0x18a54`, `0x10230` or `0x1b05c`) with the returned ROM command,
then with `0x439ac` → `0x43888` → vector-15 UART →
68000 L3 → dispatcher → SCSP key-on. Do not stage synthetic audio
packets and call that a recovered gameplay event.
