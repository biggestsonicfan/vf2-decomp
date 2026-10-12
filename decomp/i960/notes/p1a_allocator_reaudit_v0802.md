# P1a: Loop-B + allocator re-audit per entry (v0802)

## Scope

"Per entry" interpreted as per dispatch class entry (0x80, 0x90,
0xa0, 0xb0, 0xc0, 0xe0). Loop-B (0x902) is a known
"post-refusal or dead-entry territory" target (v0797), not
currently modeled in the C; the re-audit confirms the C correctly
refuses before reaching it.

## Per-entry audit (current C state vs silicon)

| entry | C function | C behavior | silicon (per v0795/v0797/v0799) | C correct? |
|---|---|---|---|---|
| 0x80 | `sound_board_dispatch_80` (line 732) | computes `base = (command&0x7f) + desc[4..5]`, writes low byte to `$150f`, refuses UNSUPPORTED | 0x10d2: only matchable shape (drain matches desc[1..2].w == channel <= 15) takes the low arm, which is a stale branch into mid-lea (bcs lands inside the row-table lea). Everything past (row/$1515/voice update) is unreachable. | YES — refuse is correct |
| 0x90 | `sound_board_dispatch_90` (line 761) | for 0x90-class: `base = command + desc[4]`, writes low byte to `$150f`, reads mode byte from 0x60c6a6, refuses UNSUPPORTED. Default classes use payload as head. | 0x85a/0x858: low arm (desc[2] < 0xf0, only matchable) is a stale branch into mid-lea. High arm needs desc[2] >= 0xf0, cannot match. Everything past (scan/compact/kill/alloc, 0x8d2 mid-instruction trap) is post-trap. | YES — refuse is correct |
| 0xa0 | `sound_board_dispatch_a0` (line 816) | refuses at entry UNSUPPORTED | 0x1e74: body splits on `cmp.l d2,d0` with d2 never loaded (d2.upper is main-loop/driver residue, mis-splits by history, stale arm stores through residue-a2). Unmodelable. | YES — refuse is correct |
| 0xb0 | `sound_board_dispatch_b0` (line 1017) | entry 0x3a -> `sound_board_sample_upload`. Other entries default-rts (consume, no effects). | 0x13cc: only 0x3a is live (-> 0x1ac0 sample-upload, 19-target table measured). Rest default-rts. | YES — matches |
| 0xc0 | inline in `vf2_sound_board_dispatch_next` (line 1131) | calls `sound_board_clear_c0_voices` with channel from command&0xf. Sets descriptor[2] = command\|0x80. | 0x1e5c: V1/V2 clears + channel zeros ($4220/$4240/word $4200), absolute SCSP (offsets 0x0b=0x1f, 0x00=0x10 — not slot key-off), ram[0x2000]=0, word-match (voice[4].w == channel), unconditional --151e. | YES — matches (v0795 recovered, v0797 corrected) |
| 0xe0 | `sound_board_dispatch_e0` (line 931) | for channel < 9: scans voice table, calls `sound_board_e0_voice_tail` for matching voices. channel >= 9 early return. | 0x12b6: per-voice tail with shaped = ((voice[3]&0xf0)<<7) + picker) & 0x7bff, T2 = ram[0x201a+d4] timer. channel >= 9 falls into B0 primer (default-rts). | YES — matches (this session's TRUE add.B fix at 0x135e closes the e0 tail pin) |

## Loop-B (0x902) — not modeled in C, by design

Per v0797: "Loop B (0x902 primer tail, $150e&0xf/$152e gates) and
the 0x1c0a B0-sub-dispatcher + 0x1c24/0x1d5e scans: post-refusal
or dead-entry territory (only B0 0x3a live; 0x1d5e entered once
via beq from the 0x7e arm). Future 90/80-voice targets, not this
slice."

Loop-B is the voice allocator that feeds the voice-start chain
(0xba0, 0xbbc, 0xbc0, 0xbd4, 0xc1e, 0xc6a per v0799c). It is
triggered by the $150e&0xf / $152e>=5 arms with V2/V3 keys and
voice[5]=5 set. The C correctly refuses before reaching Loop-B
(0x80 and 0x90 dispatch entries refuse with UNSUPPORTED, and
the silicon's Loop-B is unreachable from the C's covered
dispatch paths).

No C-side modeling of Loop-B is required. The re-audit
confirms the C's current refusal behavior is correct.

## 0x2242 walk entries (P2 boundary, noted for next slice)

P2 is "0x2242 walk entries + tails + pump wiring." Per the
v0799c note, the 0xc30 block (0xc1e-0xc6a) is the
sample-resolution + voice-start trigger chain that Loop-B
feeds. The 0x2242 walk is the main-loop head that the L2
driver's worker-forward (0x21ba +0x0c==0 -> 0x2242) enters
with the preempted a0. Per v0797: "Unmodelable in C (needs
preempted register state): boundary."

The P2 slice is a separate investigation. P1a does not
touch it.

## Conclusion

P1a re-audit: **no C-side fixes needed**. The current C's
refusal behavior for all six dispatch class entries is
correct per the v0795/v0797/v0799-series silicon evidence.
Loop-B (0x902) is correctly not modeled (post-refusal
territory, future 90/80-voice target).

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p1a_allocator_reaudit_v0802.md` — this note
