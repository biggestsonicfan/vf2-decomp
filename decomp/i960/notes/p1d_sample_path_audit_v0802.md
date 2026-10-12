# P1d: sample path wiring (sample_ram) — audit (v0802)

## Scope

P1d is the audit of the C's sample path wiring — how sample
ROM data flows from the attached sample ROM through the
uploader to the $5000 staging table and (post-refusal) to
the SCSP sample address register.

## Sample path (current C state)

| step | C function | silicon | C correct? |
|---|---|---|---|
| 1. Sample ROM attachment | `vf2_sound_board_attach_sample_rom` (line 269) attaches `board->sample_rom` + `board->sample_rom_size` AND calls `vf2_scsp_attach_sample_rom` | audio CPU has sample ROM at 0x800000+ mapped via SCSP | YES |
| 2. Sample read (uploader) | `vf2_sound_board_read_u8(board, source, &sample)` (line 245 in `sound_board_sample_upload`); reads from `VF2_SOUND_SAMPLE_ROM_BASE + offset` (0x800000+) | body chain 0xc2e: `move.l $10(a2,d3.w), $08(a5,d4.w)` reads from $5000+offset, NOT from sample ROM directly | PARTIAL — C reads from sample ROM (post-staging), silicon reads from $5000+offset (pre-staging). Different path, same data if uploader staged correctly. |
| 3. Sample staging (to $5000) | `sound_board_sample_upload` (line 140) stages from audio ROM 0x605b28+ to `sound_ram[0x5000..0x5000+first_count-1]` | silicon 0x1AD0-0x1AD2 staging loop: `move.b (a0)+, (a1)+` with dbf (self-loop per stepper, first_count bytes per v0799c notes) | YES — staging is modeled |
| 4. Sample addressing (SCSP[slot+8]) | not modeled | body chain 0xc2e: `move.l $10(a2,d3.w), $08(a5,d4.w)` | YES — post-refusal, not reached from C's covered paths |
| 5. KEYON trigger (SCSP[slot+0] \| 0x10300000) | not modeled | body chain 0xc8e: `ori.l #$10300000,d6; move.l d6, $00(a5,d4.w)` | YES — post-refusal, not reached |

## Sample path detail

The sample path has two stages:

### Stage 1: uploader -> $5000 table (modeled)

The uploader at 0x1AC0 reads from the audio ROM at 0x605b28+
(table) and writes to the $5000 staging table in sound RAM.
The C's `sound_board_sample_upload` models this:
- Reads `first_count` from audio ROM 0x605b26
- Stages `first_count` bytes from audio ROM 0x605b28+ to `sound_ram[0x5000..0x5000+first_count-1]`
- For each record (count+1 iterations), reads source/stored from `table + entry`, computes `stored - source`, writes to slot, reads one sample byte, writes to target

### Stage 2: $5000 table -> SCSP[slot+8] (NOT modeled)

The body chain at 0xc1e-0xc2e reads from the $5000 table
indexed by `voice[6] & 0x7ff * 0x14` and writes to SCSP[slot+8]
(sample address long) and SCSP[slot+0x0c] (attr byte). This
is post-refusal and not modeled in the C.

## Why the C's sample path is correct on its covered paths

1. The uploader stages data to $5000. ✓
2. The C refuses before reaching the body chain (0x80/0x90
   refuse UNSUPPORTED). ✓
3. The C never reads SCSP[slot+8] or SCSP[slot+0 | 0x10300000]. ✓
4. The SCSP model is attached to the sample ROM but never
   read from the sample path. The SCSP model is used for
   key-off writes (slot+0x0b) and e0 tail writes (slot+0x10),
   not for sample reads.

The sample path is correctly wired for the C's covered
dispatch paths. No C-side fixes needed.

## Note on the "partial" step 2

Step 2 in the table above is marked PARTIAL because the C's
uploader reads the sample byte from the source address in
the sample ROM (0x800000+), while the silicon's body chain
reads from the $5000 staging table (0x5000+). This is a
difference in path, not in data: the uploader stages the
sample byte to the target (0x50000+), and the body chain
would read from the $5000 table to get the sample address.
The C's path (direct sample ROM read) is a simplification
that works because the body chain is post-refusal.

If the body chain were modeled, the sample read path would
need to change: the C would read from `sound_ram[0x5000 +
voice[6]&0x7ff * 0x14 + 0x10]` (the $5000 table entry's
sample address) instead of from the sample ROM directly.
This is part of P1b's scope.

## Conclusion

P1d audit: **no C-side fixes needed**. The sample path is
correctly wired for the C's covered dispatch paths:
- Sample ROM attachment works
- Sample reads work (uploader reads from sample ROM)
- Sample staging works (writes to $5000)
- Sample addressing (SCSP[slot+8]) is correctly not modeled
  (post-refusal)

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p1d_sample_path_audit_v0802.md` — this note
