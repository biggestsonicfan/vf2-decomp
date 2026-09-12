# v0322 (measurement only): coli long-body early exit at 0x230a0

## Measured

`g8+0x1a4` bit 16 set with `g7+0x821 == 0` exits the long body at
`0x230a0` (counter-- at `g7+0x1234`) in **15** reference steps
(unit would be 16 including the completed ret).

Path:

```text
0x225f0  ldob g7+0x821
0x225f4  ld   g8+0x1a4
0x225f8  cmpibne 0, r5        ; r5==0 → fall through
0x225fc  bbs  3, r6           ; bit 3 clear → nt
0x22600  bbs  15, r6          ; bit 15 clear → nt
0x22604  bbc  16, r6          ; bit 16 set → nt
0x22608  cmpibne 4, r5        ; r5=0 != 4 → 0x230a0
0x230a0  ld/lda/st g7+0x1234  ; counter--
0x230b8  ret
```

Net counter effect: caller prefix `counter++` then early-exit
`counter--` leaves `g7+0x1234` unchanged.

`g8+0x1a4` bit 14 set → **289** (counter++ at `g8+0x6d9`, board bit 9
taken → `0x22e24`). Bit 4 set → **301**. `g7+0x828` bit 14 set →
**293**.

## Not recovered

A flags-region restructure to admit the early exit changed the warm
body count and failed the 249/302 pins. Kept fail-closed pending a
minimal insert that preserves the existing `+2 +4` warm accounting.

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x10000 \
  --until 0x000230b8 --max-steps 500
```
