# E1f-c: 0x11d0 voice subroutine, all four arms (9/10 arc)

Status: `sound_board_voice_sub_11d0` + five-round differential
green in-file (whole binary, 0 FAIL). Full suite in the
commit validation.

## Recovery

Called per scan match with (voice, channel byte -- d1 upper
is entry residue but every silicon use reads .w/.b only).
Three exits, all rts (caller continues its dbf):

- Defer (chantable 0x4220+channel bit7): +2 |= 0x02 only.
- Key+L1-arm (+0xb != 0): SCSP[slot] byte = 0x10 (V1 key
  form -- primes, no KEYONB) + `bset #0` (THE L1 trigger
  population this arc was built to find).
- Teardown (+0xb == 0): key-off pair (SCSP[slot+0xb]=0x1f,
  SCSP[slot]=0x10), stream[slot]=0, +2=0, then the
  L1-shaped arms: normal (+9 snap, $151e--, +9 rebalance) /
  release (+5 snap, +9/+5=0, $152e--, +5 rebalance over
  0xff-aged). Rebalance loops duplicate `maintain_voices`
  deliberately (per-site explicit style; teardown sites
  never share L1's live counters).

Per-op OOB pins (0x11f2/0x1200/0x121e). No `key_off` reuse:
it lacks the SCSP[slot]=0x10 store. d2/d0 snapshots are
byte-compare-only (v0821 rule), so no residue inputs.

## Differential (E1f-a row + staged voices/channel)

| round | shape | C post | S steps |
|---|---|---|---|
| 0 | teardown-normal | +2=0, 10/1f, 151e=ff | 534 |
| 1 | key+L1-arm | +2=0x41, 10/00 | 330 |
| 2 | defer | +2=0x42, untouched | 326 |
| 3 | teardown-release | +2=0, 10/1f, 152e=ff | 470 |
| 4 | miss-all | consume, v2 0xc0 kept | 309 |

Every round: C OK + consume, S drain exit 0x265c,
full-RAM (drain exclusions) + full-SCSP equality. The C
predictions matched silicon first try on all five -- the
map was exact before measurement.

## Consequence

The E1f-b 0x11c0 door is gone (no path refuses there; the
83-step arrival pin is void -- recorded, not deleted from
history). E1f-d remains: drive a bset-#0'd voice through
`maintain_voices` to expiry -> key-off (round 1 produces
exactly that voice shape: +2=0x41, +0xb=5).
