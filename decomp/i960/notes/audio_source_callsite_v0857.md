# v0857 — i960 sound producer call-site census

Static ROM investigation only. The user-local supported VF2 ROM set was
reconstructed in the manifest's LOAD32_WORD order and scanned with
the signed relative CTRL-call rule from src/i960/decoder.c.

| Target | Direct aligned CTRL calls |
|---|---:|
| 0x43888 queue filer | 80 |
| 0x438ec common queue tail | 8 |
| 0x439ac dedup | 15 |
| 0x43950 per-fighter driver | 5 |
| 0x18a54 Model 2A sound table dispatch | 2 |
| 0x437d0 UART IRQ entry | 1 |
| 0x43800 drainer body | 0 (0x437d0 falls through) |
| 0x43abc task flusher | 0 (scheduler calls indirectly) |

All **15 direct calls** to 0x439ac are immediately followed by a
direct call to 0x43888 in the binary. The dispatcher 0x18a54 has
two direct callers at **0x1615c** and **0x2308c**. The latter is
the *not-taken, unmeasured* negative 0x50028 branch documented in
decomp/i960/notes/fa_coli_22d8c_g05_v0339.md.

These are aligned opcode candidates, not runtime trace or evidence
of naturally audible effects. CALLX is not scanned. 0 direct calls
do not imply a dead function (IRQ fall-through and scheduler).

Reproduce without committing ROMs:

    python tools/python/test_audio_callsite_census.py -v
    python tools/python/audio_callsite_census.py /path/to/vf2.zip
    python tools/python/audio_callsite_census.py /path/to/vf2.zip --json

The next acceptance gate is a real pre-contact state with per-frame
IRQs, observing a queue write caused by gameplay and then the
i960 → UART → 68000 L3 → SCSP path in the C/stepper differential.
No C native runtime/SCSP functionality was claimed or modified.

## v0857b — passive trace parser contract correction

The queue provenance tool now handles long vf2probe traces without building
a full step-number-to-PC dictionary. It buffers only relevant writes
until the matching instruction step and retains unknown PCs when the
step event is missing. Full queue entries require an aligned 4-byte
write; partial writes and boundary overlaps have distinct labels and
are excluded from "full packet" writer summaries. Six regression
cases cover callback ordering, missing steps, partials, malformed
records, and a 30,000-step input stream. Combined local Python
validation: 22/22 test cases. A natural pre-contact C/stepper
differential and the native C suite are still unrun here.
