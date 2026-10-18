# v0861 — Three ROM-proven i960 sound selection routes

Status: **ROM opcode recovery and passive attestation tooling**;
not a naturally triggered combat sound, C runtime migration or
C-vs-68000 differential.

## Recovered original selection structure

Three separate i960 routes load the character-type byte at
`fighter + 0x1b1` into `r4`, look up `table[r3]` from
`0x0201e44c`, and fetch `row[r4]` into `r5`.
The indexed loads are checked at the opcode-bit level (MEMA
fighter byte and MEMB absolute/base-index loads, x4 scale).

| Route | fighter-type byte | row pointer | selected command | fighter register |
| --- | --- | --- | --- | --- |
| fighter_driver | 0x18a8c | 0x18a90 | 0x18a98 | g7 |
| round_selector | 0x10258 | 0x1027c | 0x10284 | r7 |
| state_selector | 0x1b070 | 0x1b074 | 0x1b07c | g7 |

The first route belongs to the original 0x18a54 dispatcher.
After the command selection, the same ROM has direct calls at
0x18ab4 → 0x43888, 0x18abc → 0x439ac, and
0x18ac0 → 0x43888. This is **static caller evidence**, not
proof that any branch is taken in gameplay.

The matrix from v0860 has 18 profiles x 11 columns with 107
nonzero slots (98 distinct command words). A ROM-verified
index relationship does not by itself name a game move or
prove SCSP key-on.

## New instrumentation

`tools/python/audio_selector_provenance.py` verifies all three
opcode forms against the user's local VF2 ROM, then optionally
consumes genuine `vf2probe --trace --memory-trace` JSONL.
For each observed selection, it requires:

1. one character/type byte read at the original instruction;
2. a row-pointer read from the known profile table, matching
   its exact ROM-stored address/value;
3. a 32-bit command read at row-pointer + type*4, matching the
   original ROM's command;
4. continuous step/IP trace (including true branches), no
   missing instruction or silently mixed invocation;
5. queue writes, if any, matched by *value and time*, labeled
   **temporal candidates**, not causal proof.

Null selections are not matched to a zero queue write. The
tool limits retained samples and candidates, but maintains
counts of additional verified selections.

Use:

~~~sh
export VF2_ROM_ZIP=/path/to/vf2.zip
python -m unittest discover -s tools/python -p 'test_audio_*.py'
python tools/python/audio_selector_provenance.py /path/to/vf2.zip --json
python tools/python/audio_selector_provenance.py /path/to/vf2.zip \
  --trace out/real-precontact.jsonl --json
~~~

**Validation this slice:** the local full Python audio-tool
suite, including the ROM-backed cases, passed 67/67.
There are 17 new selector-tool unit cases (16 ROM-free and
one ROM-backed). Raw traces and proprietary ROM assets
were never committed.

## Next evidence gate

A naturally reached pre-contact trace must show a selection
followed by a queue write for that same word, then vector-15
UART output, 68000 reception and SCSP key-on. A mere trace
coincidence cannot substitute for the C-vs-stepper differential.
