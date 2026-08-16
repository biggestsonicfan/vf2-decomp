# selector17 bit-7 index11 — EXIT TEST MODE

Entry slot `0x0005ff00` targets `0x0005ef60`; the flagged phase index is `0x8b`.

The ROM handler has two persistent states rather than a conventional menu:

- `a5 == 0`: first visit. It updates the game meter, recomputes the 15-byte coin/config CRC through `0x5ff54 -> 0x9480`, clears the diagnostic plane, renders the exit-mode diagnostic record, stores a 320-frame countdown at `0x00500024`, and changes `a5` to `0xff`.
- `a5 == 0xff`: decrements the countdown. Positive values return normally. A non-positive value executes the terminal reset path at `0x5f07c` and branches directly to boot entry `0x000000b0`.

## Static-RAM backup-mode warning

The previously unsupported first-visit branch is selected when bit 0 of `0x00500171` is clear. The ROM does not abort; after the ordinary first-visit screen it renders two inline strings via the `0x9444` text helper:

- `0x5f00c`: `STATIC RAM IS 'BACK-UP MODE'` at tile destination `0x01000f26` (row 30, column 19).
- `0x5f03c`: `AND YOUR CHANGES ARE INVALID !!` at `0x010010a4` (row 33, column 18).

Direct ROM measurements from `0x5ef60` to the caller return show:

| path | raw instructions | raw calls | full frame bridge accounting |
| --- | ---: | ---: | ---: |
| first visit, normal | 13,259 | 25 | 13,286 / 27 / 28 |
| first visit, backup mode | 13,817 | 29 | 13,844 / 31 / 32 |
| countdown > 0 | 599 | 23 | 626 / 25 / 26 |
| terminal countdown | 13,171 to `0xb0` | 25 | 13,194 / 27 / 25 |

The +558 instruction / +4 call delta on backup mode comes from the two warning-text helper invocations and their nested work. The recovered bridge renders both strings and accounts those four nested procedures explicitly.

## Terminal reset

At countdown expiry the ROM:

1. writes `0x8000` to `0x00500082`;
2. clears bit 15 from the two words at `0x0100a00c`;
3. clears `0x0050009c`;
4. clears the 64x48 diagnostic tile plane;
5. clears video-control bits 0 and 1 through the pointer at `0x0050081c`;
6. zeros the four input words at `0x00500700..0x0050070c`;
7. writes zero to `0x00e80004`;
8. calls `0x6116c`, leaves the RESET sentinel words in `0x0059cfe0`, and branches to boot entry `0xb0` rather than returning through the phase wrappers.

No ROM or snapshot bytes are stored in the repository; measurements were made locally against the owned VF2 v2.1 ROM set.
