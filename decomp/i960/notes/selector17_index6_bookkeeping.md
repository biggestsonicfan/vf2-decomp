# Selector 17 bit-7 index 6: BOOKKEEPING

Selector-17 index 6 is entered naturally from the completed COIN ASSIGNMENT
exit. The parent menu leaves `a4 = 0x05`; applying the same combined
SERVICE+TEST input (`0x00500704 = 0x1004`) used for the previous selector
transition produces `a4 = 0x86`. After the scheduler consumes that input, the
first clean frame begins at `0x0000a6c0` with `a4 = 0x86`, `a5 = 0`,
`a6 = 0xff`, `a7 = 0xff`, and zero navigation flags.

The first observed page is `BOOKKEEPING 1/5`. Its first render consumes 15,309
i960 instructions, 27 calls and 28 returns and advances `a5` from 0 to 1. The
screen is headed `GLOBAL DATA` and contains COIN CHUTE #1/#2, TOTAL COINS,
COIN CREDITS, SERVICE CREDITS, TOTAL CREDITS, TOTAL TIME, PLAY TIME,
PLAY TIME RATIO(*1000), TOTAL GAME COUNT, 1P/VS counts, several average-time
fields, and the footer `PUSH SERVICE BUTTON TO CONTINUE.` /
`PUSH TEST BUTTON TO EXIT.`

The page renderer is centered around the code near `0x0005cbe8..0x0005cccc`.
It walks descriptor records and delegates value formatting to the generic helper
at `0x0006132c`. The GLOBAL DATA descriptors include 64-bit counters and ratio
calculations. In particular, `0x000613e8..0x00061424` loads two register pairs,
converts long integers with `cvtilr`, compares against 0.5, divides/multiplies as
real values, converts back with `cvtri`, and renders the resulting integer.

Scouting page 2 exposed that `cvtilr` was decoded but not executed by the local
i960 interpreter. Native support has therefore been added in the executor as a
signed 64-bit register-pair to real32 conversion, alongside the existing
`cvtir`, `mulr`, `divr`, `cmpr`, and `cvtri` support. This is an architectural
executor improvement rather than a BOOKKEEPING-specific bypass; page recovery
continues from the same ROM-backed snapshot after that opcode becomes executable.
