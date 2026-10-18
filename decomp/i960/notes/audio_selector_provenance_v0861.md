# v0861 — ROM-proven selector index semantics and passive run verification

**Proven by supported VF2.1 Model 2A i960 ROM, not by guessed game labels.**

Three original i960 call-site corridors select command words through the
18 × 11 main_data matrix:

| Corridor | ldob fighter type to r4 | ld pointer table indexed by r3 | ld command at pointer[r4*4] |
|---|---|---|---|
| 0x18a88 | 0x18a8c from g7+0x1b1 | 0x18a90 to r3 | 0x18a98 to r5 |
| 0x10230 | 0x10258 from r7+0x1b1 | 0x1027c to r15 | 0x10284 to r5 |
| 0x1b060 | 0x1b070 from g7+0x1b1 | 0x1b074 to r15 | 0x1b07c to r5 |

This **corrects the earlier generic "profile / position" interpretation**:
the 18 pointer-table rows are selected by an event/selector value in r3;
the 11 columns are selected by the fighter's type byte in r4.
The row's named move/event and the actual fighter-type-to-name mapping are
not assigned without live observation.

Both MEMB indexed loads use scale=2 (×4): first absolute-index r3,
then base-index r4. The destination of the first is r3 or r15, and
the second always returns a command candidate in r5. All nine
instruction encodings and the absolute extension 0x0201e44c were
verified against the user's actual i960 program ROM. These are
static, instruction-level facts; none prove the corridor is reached.

## Added

- `tools/python/audio_selector_provenance.py` rejects different
  opcode shapes or table bases, and optionally parses *real*
  `vf2probe --trace --memory-trace` streams.
- The passive trace matcher observes a character-type read, follows
  the actual pointer-table bus read, then validates the indexed
  command fetch against the user's original main_data ROM. It only
  accepts matching widths, PCs, addresses, register-derived indices,
  and command value; interrupted/incomplete streams fail closed.
- A later full aligned 32-bit queue write with an equal word can
  be marked a **temporal candidate** (limited instruction distance).
  Equal packet values alone do not prove provenance, gameplay input,
  UART reception, SCSP key-on, or audible sound.

## Reproduce

~~~sh
VF2_ROM_ZIP=/path/to/vf2.zip python -m unittest discover -s tools/python -p 'test_audio_*.py'
python tools/python/audio_selector_provenance.py /path/to/vf2.zip --json
python tools/python/audio_selector_provenance.py /path/to/vf2.zip \
  --trace /path/to/real-precontact.jsonl --json
~~~

Local: **64/64 Python tests passed**, including the current
ROM-backed selector and matrix cases. No C suite was re-run, no
naturally reached pre-contact gameplay sound was captured, and
no full 68000/SCSP differential was claimed. Preserve all proprietary
ROMs, snapshots and full trace streams off Git.
