# v0318: coli long-body `g8+0x1a4` bit 13 early-join

## Verdict

`g8+0x1a4` bit 13 set joins the cascade when `g8+0x5b8` bit 0 is also
set (`ld` / `bbc 13` not taken / `ld +0x5b8` / `bbs 0` taken). Adds
**2** instructions (249 → **251** on the v0288 drive).

Deeper bit-13 arms (`+0x5b8` bit 0 clear, `+0x1a4` bit 3, scan-byte
2/5/6, etc.) remain fail-closed.

Also corrected a cascade gate that had been checking bit 13 at the
`0x22a28` site; the ROM tests **bit 8**. Bit 8 set still fails closed.

## Drive

`out/coli-225cc-entry.vf2snap` with `g8+0x1a4 = 0x2000`,
`g8+0x5b8 = 1`.

## Proof

- unit test in `test_coli_225cc_long`: 251
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
