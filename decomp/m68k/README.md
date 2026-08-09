# Motorola 68000 sound-program recovery

`epr-17574.30` is the 512 KiB sound program loaded with 16-bit word swapping.

This subtree will eventually contain:

- function and symbol maps;
- SCSP command protocol notes and the recovered CPU-visible register/sample/MIDI boundary;
- recovered C translation units;
- differential audio-driver tests.

The `vf2m68k` tool now validates the audio ROM's big-endian vector table and
disassembles a bounded, explicitly supported subset of 68000 instructions,
and preserves unsupported opcodes as data rather than silently guessing.

The ROM confirms the Model 2 sound-board map used by the host boundary:
sound RAM at `0x000000`, SCSP registers at `0x100000`, the sound-control latch
at `0x400000`, audio ROM at `0x600000`, and sample ROM at `0x800000`.
The shared interrupt handler at `0x4fe` and the SCSP voice-maintenance handler
at `0x5fe` are instruction-aligned. The latter scans 32 entries at the
`0x1800` sound-RAM table, applies the two observed status/countdown paths, and
updates the SCSP-facing control bytes.

The recovered voice-maintenance loop is now implemented at field level. For each
16-byte entry it checks status bit 0, decrements byte `+0xb`, and, when the
counter expires, selects one of two paths using status bit 3. Both paths clear
the entry status and the SCSP key/control byte at `0x1000`; the first path also
decrements the shared `0x151e` counter and ages matching entries by byte `+9`,
while the second clears bytes `+5` and `+9` and ages matching entries by byte
`+5`, using `0xff` as the inactive marker. The handler writes `0x3c0` and
`0x80` to the sound-board control registers on entry and returns through the
saved register mask at `0x6e0`.

The command dispatcher at `0x7a6` is also bounded through its table lookup:
it masks the command cursor at `0x1508`, advances it by four bytes, scans 20
descriptors at `0x3000`, and calls the matching handler through `0x81a`.

The dispatcher masks the command byte to its high nibble and has verified
branches for `0x90` (inline path), `0x80` -> `0x10d2`, `0xe0` -> `0x12b6`,
`0xb0` -> `0x13cc`, `0xc0` -> `0x1e5c`, and `0xa0` -> `0x1e76`.

The `0x80` path is bounded through its lookup preparation. Values at or above
`0xf0` select the `0x6032c6` pointer table after subtracting `0xf0` and
scaling the masked command by four; the selected record is then indexed in
six-byte units by the masked voice number. The alternate path uses the
`0x6053aa` table, walks six-byte records until the command range contains the
requested value, and derives the same `0x1515`/`0x1516` lookup pair. The common
tail scans the 32 voice records and calls `0x11d0` only when bytes `+3`, `+4`
and `+6` match the selected command, voice and table value and status bits 4
and 1 are clear.

The `0xe0` path at `0x12b6` is likewise bounded through its voice-selection
and SCSP-write helper. It combines the command byte and the next stream byte
into a masked table index, selects a four-byte pointer from `0x602b26`, and
uses the resulting byte value to update the per-channel word at `0x3200`.
For channels below nine it scans the 32-entry table at `0x1800`, matching
byte `+4` and a live status byte, then the helper sign-extends the channel
offset and selects data from the `0x602676` and `0x60c726` pointer tables.
The observed terminal writes are the voice word to `0x4010(a5)` and the
selected SCSP word to `0x401a(a2)`.

The nonzero-stream `0x90` path now also implements its exact no-live-voice
return: the 32 voice records are filtered by the derived lookup byte, channel,
lookup word, and status bits, with status bit 7 cleared before the live helper
boundary. Matching live records use both observed `0x11d0` branches: when
`0x3220 + channel` has bit 7 set it sets voice status bit 1, while the slow
`0x11e8`–`0x12b2` path performs the SCSP key-off, clears normal/release status,
and ages the corresponding `0x151e`/`0x152e` countdown group. The `0x0f14`
allocator prefix is now recovered for populated sample-table records: it
selects the first inactive voice, writes the normal/high-selector voice fields
and lifetime counters, and reproduces the observed SCSP control, envelope,
pitch, pan and modulation writes. The deeper sample-table copy and
sample-address programming remain explicitly guarded when the source record is
not populated. The zero-stream variant still enters the shared lookup/no-live
boundary as in ROM; its live allocator tail remains open.

The `0xb0` entry at `0x13cc` is a compact jump-table dispatcher: it masks the
command index to seven bits, scales it by four, forms a PC-indexed table
address with `lea $06(pc,d2.w),a1`, and jumps through `a1`. The table contains
the recovered handler entries at `0x15de`, `0x164c`, `0x16ba`, `0x174c`,
`0x181c`, `0x18a8`, `0x18b6`, `0x1912`, `0x194a`, `0x1986`, `0x19c2`,
`0x1a48`, `0x1a76`, `0x1a9a`, `0x1ac0`, `0x1b4c`, and `0x1c0a`; unused slots
return through `0x13dc`.

The small `0xc0` handler at `0x1e5c` is also complete at the boundary: it
sets status bit 7 for command values at or above `0x70`, stores the resulting
byte at voice-entry offset `+2`, and calls `0x1de0`. The `0xa0` path at
`0x1e76` selects a four-byte pointer from `0x608e12`, compares the streamed
record against its table limit, and either returns or allocates a 32-byte
voice descriptor from the `0x2000` pool. The allocation path clears 32 status
bytes, stores the source pointer at offsets `+4` and `+8`, sets state word `+2`
to one, and marks descriptor bits 3 and 7; the alternate path scans six
descriptors at `0x2020` before using the same initializer.

The first `0xb0` handlers are now instruction-aligned as well. Entries
`0x15de` and `0x164c` update the two sixteen-channel SCSP control tables at
`0x3220` and `0x3240`, then scan the 32 voice records and write the packed
voice value at `0x4013(a5)`. Entries `0x16ba` and `0x181c` scale duration
bytes with the shared `0x151f` factor and update `+0xa` in the matching
descriptor, with the zero path writing `0xff` to `0x400d(a5)`. Entry `0x174c`
(payload-`0x0a`) uses the PC-indexed table at `0x179c` to update voice byte `+6` and the
packed `0x4016(a5)` register. Entry `0x18b6` copies a 160-byte stream into
the ten descriptors at `0x3000`, then recomputes their duration bytes; the
`0x1912`, `0x194a`, and `0x1986` handlers update descriptor bytes `+0xd` and
`+0xc`. The later entries cover channel-data copies, descriptor allocation,
and shared cleanup through `0x1de0`.

The stream interpreter rooted at `0x1f7c` now covers the bounded normal,
`0x20`, and `0xc0`/`0xd0` packet paths, plus the counted `0xf7` skip escape
the F0 wait/search record through its ROM `0xf7` sentinel, the bounded
non-pointer FF skip record, and the ordinary/`0xfffffff1` pointer re-entry
records. It decrements
descriptor timing bytes, reads the source pointer at `+4`, writes decoded
four-byte ring entries, advances the ring cursor by four, increments the
packet counter at `0x1506`, and reloads continuation timers. The remaining
high-bit B0 and F0 escape/control paths and live-voice tails remain explicit
unsupported boundaries; the exhausted and `0xfffffff2` clear sentinels are
modeled. The ordinary and `0xfffffff1` cases at `0x215a` now recover their
descriptor chain/source longword writes and re-enter the common packet decoder
with a bounded cycle guard.

The small helper at `0x1f04` clears a 32-byte descriptor header area and then
returns; its `suba.l #$20,a4` epilogue is now decoded explicitly rather than
being mistaken for table data.

The 68000 decoder covers the immediate and register forms of `btst`, `bchg`,
`bclr`, and `bset`, indexed address extensions, `subi`, `addi`, `mulu`,
register shifts, `ext`, `or`, `neg`, `tst`, `cmp`/`cmpa`, `sub`/`suba`,
address-register quick adds, and the memory-to-memory forms used by this path.
Full command decoding, voice programming, and hardware-accurate SCSP FM/DSP
and hardware-accurate envelope timing remain open. The portable `vf2_sound_board` API now
exposes the ROM-backed sound RAM, SCSP register/MIDI boundary, deterministic
PCM slot renderer, control latch, audio-ROM and direct sample-ROM windows.
`vf2_sound_board_maintain_voices` composes the instruction-aligned 0x5fe
interrupt transition, including timer expiry, normal/release aging, shared
counter updates and SCSP slot key-off.
`vf2_sound_board_dispatch_next` now consumes the ROM-format command-ring entry,
matches the 20 descriptors, and implements the `0xc0` status/voice-cleanup path
plus the no-live-voice `0xe0` table/channel-result path and the no-live-voice
`0xb1`/`0xb2` channel-control jump-table entries, the `b7` and payload-`0x10`
descriptor-duration entries, descriptor-field entries `0x29`–`0x2b`, and the `b3` cleanup entries
`0x40`/`0x7d`/`0x7e`/`0x7f`, plus the no-live-voice `0x80` lookup boundary;
the nonzero-stream `0x90` path now prepares its ROM-derived descriptor and
lookup fields, including the matching-voice `0x11d0` slow cleanup boundary,
and the populated-table allocator prefix. Live `0xb1`/`0xb2` voices now also reach the
ROM's packed SCSP `+0x13` writes at `0x15de`/`0x164c`, preserving the
complementary bit fields. The `0xa0` path now resolves its
two-level `0x608e12` stream table and initializes the primary or alternate
32-byte stream descriptor. The `0x90` sample-table allocator tail beyond the
recovered prefix, the zero stream variant and other command families remain
explicit unsupported results.
`vf2_sound_board_emit_command` exposes the corresponding producer cursor and
packet-count update used by the ROM stream interpreter.
