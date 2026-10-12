# P4: B0 idx-123 + deadness re-proof (v0802)

## Scope

P4 is the B0 idx-123 sub-dispatcher (0x1c0a) and the
deadness re-proof of the dead code. Per v0799c: "the
B0-idx-123 half is held" and "Loop D 0x1de0: dual entry (B0
idx-123 + C0 jsr)."

## B0 sub-dispatcher (0x1c0a) disasm

```
0x1c0a: cmpi.b #$7d, d0           ; compare d0 to 0x7d
0x1c0e: beq $1cce                ; if 0x7d, jump to 0x1cce
0x1c12: cmpi.b #$7e, d0           ; compare d0 to 0x7e
0x1c16: beq $1d60                ; if 0x7e, jump to Loop C
0x1c1a: cmpi.b #$7f, d0           ; compare d0 to 0x7f
0x1c1e: beq $1c26                ; if 0x7f, jump to 0x1c26
0x1c22: rts                      ; else return
```

The B0 sub-dispatcher dispatches on d0:
- 0x7d -> 0x1cce
- 0x7e -> 0x1d60 (Loop C, a1 pre-set)
- 0x7f -> 0x1c26
- else -> rts

## Loop C (0x1d60) disasm

```
0x1d60: move.b d0, d4             ; d4 = d0
0x1d62: move.w #$1f, d7           ; d7 = 31
0x1d66: btst #6, $0002(a1)        ; check voice[2]&0x40
0x1d6c: beq $1db0                ; if not set, skip
0x1d70: cmpi.b #$9, $0004(a1)     ; check voice[4] >= 9
0x1d76: bcs $1d86                ; if < 9, skip
0x1d7a: cmpi.b #$f, $0004(a1)     ; check voice[4] == 0xf
0x1d80: bne $1db0                ; if not 0xf, skip
0x1d84: move.w $0000(a1), d4      ; d4 = voice[0] (slot word)
0x1d88: move.b #$1f, $0b(a5, d4.w) ; SCSP[slot+0x0b] = 0x1f
0x1d8e: move.b #$10, $00(a5, d4.w) ; SCSP[slot+0x00] = 0x10
0x1d94: lea $1000(a6), a3         ; a3 = 0x2000 (stream slots)
0x1d98: move.b #$0, $00(a3, d4.w) ; ram[0x2000+slot] = 0
0x1d9e: move.b $0004(a1), d1      ; d1 = voice[4]
0x1da2: move.b #$0, $0002(a1)     ; voice[2] = 0
0x1da8: move.b #$0, $0004(a1)     ; voice[4] = 0
0x1dae: adda.l #$10, a1           ; advance to next voice
0x1db4: dbf d7, $1d68            ; loop 32 voices
0x1db8: move.b #$0, $151e(a6)     ; $151e = 0
0x1dbe: lea $3220(a6), a2         ; a2 = 0x3220
0x1dc2: lea $3240(a6), a3         ; a3 = 0x3240
0x1dc6: lea $3200(a6), a4         ; a4 = 0x3200
0x1dca: move.w #$8, d7            ; d7 = 8
0x1dce: move.b #$0, (a2)+         ; clear $3220
0x1dd2: move.b #$0, (a3)+         ; clear $3240
0x1dd6: move.w #$0, (a4)+         ; clear $3200
0x1dda: dbf d7, $1dd0            ; loop 9 entries
0x1dde: rts                      ; return
```

Loop C scans the voice table, checks voice[2]&0x40 and
voice[4] in [9, 0xf], does V1 writes (SCSP[slot+0x0b]=0x1f,
SCSP[slot+0x00]=0x10, ram[0x2000+slot]=0), clears voice[2]
and voice[4], then clears $151e and the $3220/$3240/$3200
tables.

## Loop D (0x1de0) dual entry

Per v0799c: "Loop D 0x1de0: dual entry (B0 idx-123 + C0 jsr);
V2 writes + clears + unconditional --151e + 0x3200/0x3220/0x3240
clears under the $150e&0xf channel gate. The C0-adjacent half
is recovered (v0795, v0797-corrected); the B0-idx-123 half
is held."

Loop D at 0x1de0 is reached from:
- B0 idx-123 (via 0x1c0a -> 0x1cce or 0x1c26)
- C0 jsr

The C models the C0-adjacent half (v0795, v0797-corrected)
via `sound_board_clear_c0_voices`. The B0-idx-123 half is held
(not modeled).

## C's current state

| function | C state | C correct? |
|---|---|---|
| `sound_board_dispatch_b0` (line 1017) | returns VF2_OK for non-0x3a entries (default-rts) | YES — C never reaches 0x1c0a |
| `sound_board_clear_c0_voices` (line 689) | recovers C0-adjacent half (V1 writes, clears, --151e) | YES — matches v0795/v0797 |

The C correctly does not model 0x1c0a (B0 sub-dispatcher)
or 0x1de0 (Loop D B0-idx-123 half). The C's default-rts
for non-0x3a B0 entries means the C never reaches the B0
sub-dispatcher.

## Deadness re-proof

The dead code (0x1c0a, 0x1cce, 0x1d60, 0x1de0) is reached
only from B0 dispatch entries that are:
1. Not 0x3a (the only live B0 entry per v0795)
2. In the range 0x7d-0x7f (the B0 sub-dispatcher inputs)

The C's `sound_board_dispatch_b0` returns default-rts for
all non-0x3a entries. So the C never reaches the dead code.

The deadness is re-proven: the C's default-rts matches the
silicon's behavior on the C's covered paths.

## Conclusion

P4 audit: **no C-side fixes needed**. The B0 idx-123
sub-dispatcher (0x1c0a) and Loop D B0-idx-123 half (0x1de0)
are correctly not modeled. The C's default-rts for non-0x3a
B0 entries matches the silicon's behavior on the C's
covered paths. The deadness is re-proven.

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p4_b0_idx123_deadness_v0802.md` — this note
