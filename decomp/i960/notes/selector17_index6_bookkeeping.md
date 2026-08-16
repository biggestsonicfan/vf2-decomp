# Selector 17 bit-7 index 6: BOOKKEEPING

Selector-17 index 6 is entered naturally from the completed COIN ASSIGNMENT
exit. The parent menu leaves `a4 = 0x05`; applying the same combined
SERVICE+TEST input (`0x00500704 = 0x1004`) used for the previous selector
transition produces `a4 = 0x86`. After the scheduler consumes that input, the
first clean frame begins at `0x0000a6c0` with `a4 = 0x86`, `a5 = 0`,
`a6 = 0xff`, `a7 = 0xff`, and zero navigation flags.

The first observed page is `BOOKKEEPING 1/5`. Its first render consumes 15,309
i960 instructions and advances `a5` from 0 to 1. At the clean `a6c0` boundary
the snapshot delta is 25 nested calls and 26 returns; the earlier scouting log
included two outer wrapper calls and therefore reported 27/28. The screen is
headed `GLOBAL DATA` and contains COIN CHUTE #1/#2, TOTAL COINS, COIN CREDITS,
SERVICE CREDITS, TOTAL CREDITS, TOTAL TIME, PLAY TIME,
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
executor improvement rather than a BOOKKEEPING-specific bypass.

The recovered index-6 cut now owns both phases of BOOKKEEPING 1/5 for the
measured empty/default bookkeeping state. State `a5=0` reconstructs the static
GLOBAL DATA layout and advances to `a5=1`; state `a5=1` renders the zero-count,
zero-time and undefined-ratio forms (`----` / `--M --S`) exactly as observed.
The phase is dispatched through ROM entry `0x0005c9b8` from table slot
`0x0005fed8`, and the recovered finisher reproduces the measured CPU register,
condition-code and instruction/call poststate rather than returning through the
generic index-11 fallback. Later pages remain a separate extension of the same
state machine.

## Input state machine recovered from ROM

The common stable-page input tail at `0x0005cb70` is now recovered as well. The
five displayed pages use odd `a5` states (`1,3,5,7,9`), while page changes target
the even construction state of the destination page. Canonical forward input
`0x00500704 = 0x100` therefore maps `1->2`, `3->4`, `5->6`, `7->8`, and wraps
`9->0`. Canonical reverse input `0x200` maps `3->0`, `5->2`, `7->4`, `9->6`,
and wraps `1->8`. The construction frame then advances that even state to the
next stable odd state exactly as the ROM does.

Page 5 has an additional controller before the common page tail. The helper at
`0x00060b50` interprets canonical `0x1000` as `+1` and `0x2000` as `-1`; the
selected fighter in `a7` wraps over `0..9`. This is the lever-driven `MY CHAR`
selector described on screen. The recovered implementation preserves the ROM's
instruction deltas for normal and wrap transitions: page forward is idle +4
instructions (+5 for `9->0`), page reverse is idle +3 (+4 for `1->8`), fighter
+ is idle +4 (+5 for `9->0`), and fighter - is idle +1 (+2 for `0->9`). These
paths add no procedure calls beyond the already-accounted stable render.

The TEST exit mask in the same ROM tail is `0x04000014` and has precedence over
page navigation. The canonical TEST bit (`0x00000004`) was injected only after
the clean `0x0000a6c0` frame boundary so the scheduler could not consume it
before the BOOKKEEPING handler. Native ROM measurements from stable states
`a5=1,3,5,7,9` are respectively 16,037/95/96, 17,895/144/145,
17,391/115/116, 16,215/90/91, and 16,173/50/51
(instructions/calls/returns). All five converge on the same caller-visible CPU
poststate. The shared teardown at `0x0005f140` clears the 64x48 diagnostic tile
plane, clears bit 7 of `a4` (`0x86 -> 0x06`), restores the twelve parent TEST
MENU records plus the three common instruction records, and redraws the
BOOKKEEPING cursor. The recovered bridge now reproduces that teardown and its
measured poststate, completing selector-17 index 6 for the measured empty/default
bookkeeping configuration.

