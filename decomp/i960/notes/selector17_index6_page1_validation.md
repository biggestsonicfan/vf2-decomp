# Selector 17 index 6 validation

BOOKKEEPING 1/5 is recovered and strict-snapshot validated from clean `0x0000a6c0` boundaries. The `a5=0` layout and `a5=1` empty/default value phase consume 15309/25 and 1815/79 instructions/calls respectively, with one additional procedure return at the frame boundary. The renderer preserves fixed ROM tile attribute spans; the undefined PLAY TIME RATIO field is centered at columns 35..38 inside a highlighted 33..38 value span.

BOOKKEEPING 2/5 is now represented by the same two-phase state machine. `a5=2` lays out GLOBAL DATA 2 / TYPE-B DATA and advances to `a5=3`; its measured frame consumes 15175 instructions and 40 nested calls. `a5=3` renders the empty/default counters, average-time placeholders and 21-row four-column duration histogram; it consumes 3626 instructions and 128 nested calls. The renderer preserves the independent six-cell highlighted spans for each histogram count rather than approximating the table as plain text.
