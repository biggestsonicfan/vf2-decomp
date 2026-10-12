# v0799c - voice-alloc map-and-hold + move-An-Dn fix (slice 4)

## Correction to v0799b (supersedes its SCSP+0x4000 paragraph)

The move-Dn-dst fix re-reads 0xc8e (`2b 86 40 00`) as
`move.l d6,$00(a5,d4.w)`: there is NO SCSP+0x4000 far window. The
ext word 0x4000 is d4+0x00. Likewise 0xc9c/0xca6/0xcae are
ram[0x200a/0x201a/0x201c+d4] (a3 = 0x2000), and the e0-tail
0x13c4 is ram[0x201a+d4] (already corrected in code in v0799b;
only this note's window theory is retracted). The 0x10300000|XX
value lands in SCSP[slot+0] = control word with KEYONB set: the
live voice-START trigger, post-refusal, unmodeled. The DRAM
verdict sharpens rather than falls: SA comes from the staged
$5000 table via slot+8 (0xc2e), KEYON via slot+0 (0xc8e); DRAM
itself is never directly addressed, and the C (which stages via
the modeled uploader but never programs slots) renders
staged-silence. Refuse-before-start stands.

## move An,Dn decoder gap (fixed)

`move.l a2,d6` (0xc86, opcode 0x2C0A) printed `.word`: no An->Dn
branch existed (DnDn, AnAn, DnAn all did). Added sizes B/W/L +
ROM-byte vector. Gate: old + new vector FAILs; fixed green. With
it the 0xc82-0xc9c chain reads end-to-end coherent (sample longs
-> KEYON trigger -> stream-slot fill).

## 90/80-body chain recipe (post-refusal, for a future slice)

- 0xb76: voice-record fill (a1): [3] = $1515, [8] = $1510,
  [4] = $150e&0xf, [9] = $151e then $151e++, [0x0a] = $1518.
- 0xba0: SCSP[slot+0x0d] = volume-ish ((voice[8]*desc[0x0a])>>8,
  NOT, >>1), d4-indexed.
- 0xbbc: jsr $1062 (on the path, not yet mapped).
- 0xbc0: SCSP slot+4/+0x0c/+0x0e init (0x1fff/0/0), d4-indexed.
- 0xbd4: SCSP slot+0x12 = pitch (desc[8] + 0x3220/0x3240 table
  bits, d1 = $150e&0xf); slot+0x15/0x16 envelope bytes
  (desc[0x0c], $1511).
- 0xc1e: d3 = voice[6]&0x7ff * 0x14 indexes the $5000
  sample-slot table; SCSP slot+8 = sample address long (0xc2e),
  attr byte (0xc34); PC-table countdown -> voice[0x0b] (0xc3c);
  bra 0xc6a.
- 0xc46-0xc68: DATA run (skipped by the 0xc44 bra; linear-sweep
  phantoms inside, including ORI-shaped words, are not code).
- 0xc6a: sample resolution (a2 = table[d3], sign escape to 0xeda,
  twin path 0xd94); voice-start trigger (0xc82-0xca0);
  ram[0x201a/0x201c+d4] timer words; movem sample copy
  (a0)+ -> -(a2) 16+ longs.
- Loop-B (0x902) feeds this chain on the $150e&0xf / $152e>=5
  arms with V2/V3 keys and voice[5] = 5 set.

## B0-sub recipe (dead entries, held)

- idx-80 head 0x1c0a: d0 == 0x7d -> 0x1cce / 0x7e -> 0x1d60
  (loop C, a1 pre-set) / 0x7f -> 0x1c26, else rts.
- 0x1c24 scan: bit6 gate, V2 writes, voice[2]->d0 with bit4 test,
  voice[4]->d1 vs 0xf gate.
- Loop C 0x1d5e: V2 writes + clears + $151e = 0, no rebalance.
- Loop D 0x1de0: dual entry (B0 idx-123 + C0 jsr); V2 writes +
  clears + unconditional --151e + 0x3200/0x3220/0x3240 clears
  under the $150e&0xf channel gate. The C0-adjacent half is
  recovered (v0795, v0797-corrected); the B0-idx-123 half is held.

No src/test behavior change in this slice beyond the decoder
handler + vector. Frontier unchanged by design (map-and-hold).
