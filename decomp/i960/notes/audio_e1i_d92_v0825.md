# E1i: 0xd92 second engine arm recovered (9/10 arc)

Status: `sound_board_run_engine_key_d92` + `test_engine_d92`
differential green in-file (whole binary, 0 FAIL) + pump
rule extended to the 0xdf8 door. Full suite in the commit
validation.

## Recovery

Taken when the table+0x0c long (d0) is nonzero. d0 is
head-loaded (`move.l $0c(a2,d3.w),d0` at 0xc7a), so every
input derives -- E1b-dig's "residue" listing was a
fixture artifact (no S4 shape set +0x0c), corrected here, not
a silicon fact. Effects (~20 insns): bclr #4 voice+2; d1 =
long(table+8); the E1b slot program unchanged (d6 =
long(voice+0x0c) | 0x10300000 as the u16 pair -- the
word-fetch trap applies identically; stream[slot+0x0a] =
d5 - 0x200); the stream RMW pair (+0x16 keeps top byte, low
24 = d0 low 24; +0x12 keeps top byte, low 24 = d1 low 24;
andi.l/or.l exact); rate pair +0x1a/+0x1c (d2 + 0xeba wrap
exact); stream[slot+0x01] = 0x10. Refuses at the 0xdf8 copy
loop (byte-identical illegal-ea movem shape as 0xcb2, same
vector-4 -> 0x4fe fate): handler 0xdf8. Per E1b precedent the
S parks at arrival unstepped.

Const ripple (honest, minimal): the voice chain is mutable
RAM (`setup_author_at` already takes `uint8_t *` and writes
+2/+6 through it); only `sample_tail` + `engine_key`
promised const downstream. Both relaxed to `uint8_t *`;
ed8 keeps const (it never writes -- mutable-to-const
converts implicitly).

## Differential (a8c-hi fixture + staged gate)

Same synthetic board as the 322-step a8c-hi leg (desc
0x80/0x03/0xf5, ring {0x93,0,1}, slot 0x20) plus voice0[2] =
0x5a and table+0x0c = 0x00112233. Fixture trap documented:
d3 is NOT 0 -- setup authors voice+6 = 0x0140, so d3 =
0x1900 and the gate lives at 0x690c, not 0x500c. The first
draft staged 0x500c (pre-dispatch +6 is zero) and both sides
duly fell through to 0xcb2 (S faulted at 0xcb6 in 323
steps -- the dead loop doing its job). The +6 print caught
it; restaged; the arm matched first try:

- C: UNSUPPORTED @0xdf8, v0+2 = 0x48 (bclr 0x58 -> 0x48;
  setup authors +2 = 0x58, not the S3c comment's 0x40 --
  measured), str+0x16 = 00112233, str+0x12 = 0 (d1 = 0),
  str+0x0a = fffffe00 (d5 = 0 - 0x200), held.
- S: 0xdf8 in 334 steps, d0 = 0x00112233, d1 = 0, v0+2 =
  0x48, full voice0-record + full SCSP equality.

## Pump follow-up (E1d rule)

E1d counted only 0xcb2-with-effects as dispatched and left
0xd92 refused *because unmeasured*. Measured now with the
same shape (effects + dead-loop refuse), so `session.c`
counts `0xcb2 || 0xdf8`. Live pins unchanged (no df8 shape
on the init-state board).

## Engine head status

All three exits now verdict: bit31 -> ed8 key-on
(completes, E1c); +0x0c long -> d92 program + RMW (refuses
at the dead loop, this slice); fall-through -> E1b program
(refuses at the dead loop). Copy loops: dead-on-silicon
both, never stepped. Remaining engine-adjacent: the a0 body
(residue-dependent by measurement) and live gameplay
writers.
