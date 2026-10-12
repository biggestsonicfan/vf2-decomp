# P1c: SA/KEYON model audit + fix (v0802)

## Scope

P1c is the audit of the C's SA (sample address) and KEYON
models. Per v0799c: "SA comes from the staged $5000 table via
slot+8 (0xc2e), KEYON via slot+0 (0xc8e); DRAM itself is never
directly addressed, and the C (which stages via the modeled
uploader but never programs slots) renders staged-silence.
Refuse-before-start stands."

## Audit results

| model | C location | silicon location | C correct? |
|---|---|---|---|
| SA (SCSP[slot+8]) | not modeled | body chain 0xc2e: `move.l $10(a2,d3.w), $08(a5,d4.w)` | YES — body chain is post-refusal, not reached from C's covered dispatch paths |
| KEYON (SCSP[slot+0] \| 0x10300000) | not modeled | body chain 0xc8e: `ori.l #$10300000,d6; move.l d6, $00(a5,d4.w)` | YES — same reason |
| Staged-silence | uploader stages to $5000 (sound_board_sample_upload), no voice-start trigger | silicon stages then triggers via body chain | YES — C correctly stages without triggering |

The C's current SCSP slot writes are limited to:
- `sound_board_key_off`: SCSP[slot+0x0b] = 0x1f (key-off rate)
- `sound_board_clear_c0_voices`: SCSP[slot+0x0b] = 0x1f, SCSP[slot+0x00] = 0x10 (c0 clear)
- `sound_board_dispatch_e0` -> `sound_board_e0_voice_tail`: SCSP[slot+0x10] = shaped (e0 tail)

None of these are the SA (slot+8) or KEYON (slot+0 | 0x10300000)
writes. The C correctly does not model them because the body
chain (which contains these writes) is post-refusal and not
reached from the C's covered dispatch paths.

## Refuse-before-start policy

Per v0799c: "The C (which stages via the modeled uploader but
never programs slots) renders staged-silence. Refuse-before-start
stands."

The C's policy is:
1. The uploader (sound_board_sample_upload) stages sample data
   to the $5000 table. This is modeled.
2. The body chain (0xba0-0xc6a) would program the SCSP slots
   (SA at slot+8, KEYON at slot+0). This is NOT modeled.
3. The C refuses before reaching the body chain (0x80/0x90
   dispatch entries refuse UNSUPPORTED).
4. Result: staged data in $5000, no voice start, rendered
   silence. This matches the silicon's behavior on the C's
   covered dispatch paths.

The policy is correct and upheld. No C-side fixes needed.

## Why no fix is needed

The SA/KEYON writes are in the body chain (0xba0-0xc6a),
which is fed by Loop-B (0x902). Per v0797, Loop-B is
"post-refusal or dead-entry territory (only B0 0x3a live)."
The C correctly refuses before reaching Loop-B, so the SA/KEYON
writes are never reached from the C's covered paths.

To model SA/KEYON, the C would need to:
1. Add a dispatch path that reaches Loop-B
2. Add Loop-B modeling
3. Add the body chain functions

This is P1b's scope (200-400 LOC per the v0735 plan), not P1c.
P1c is the audit, which confirms the current policy is correct.

## Conclusion

P1c audit: **no C-side fixes needed**. The C correctly does
not model SA/KEYON (post-refusal), and the staged-silence
rendering matches the silicon's behavior on the C's covered
dispatch paths. The refuse-before-start policy is upheld.

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p1c_sa_keyon_audit_v0802.md` — this note
