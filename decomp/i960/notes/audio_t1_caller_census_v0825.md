# T1: filer caller census + triage (trigger plan)

Status: 80 `call 0x43888` SITES confirmed by whole-image byte
scan (the 8f note's "80 static callers" is sites, not
functions). Grouped in 16 clusters, g0 patterns read off the
pre-call instructions, ranked for trigger value. No src change.

## Method (reproducible)

MAINCPU image = epr-18385..88 interleaved LOAD32_WORD
(`img[4k+j] = hi[2k+1],hi[2k],lo[2k+1],lo[2k]`, validated
byte-exact at 0x3ae60/68/74 against `vf2i960 disasm`).
`call` = first-byte 0x09, disp24 = next 3 bytes BE,
target = pc + disp24 (verified: 0x3ae68 + 0x8a20 = 0x43888).
Scan both 256KB banks for target == 0x43888. `bal` (0x0b):
zero hits. Repo `xrefs`/callgraph show only the 2 sound-test
wrapper sites -- they are lower bounds (recursive-descent
coverage), not the census.

## The 80 sites in 16 clusters

- 0x43xxx x14 (43994..4403c): FLAG-FLUSHER. Repeated
  `bbc N, r3` / `ld 0x201e940[r4*4], g0; cmpobe 0, g0, skip;
  call filer` per bit + `mov r3/r4/r5, g0; call` record
  filers; state word spilled to `(g13)`. Per-set-bit command
  from a ROM table -- the probable LIVE gameplay dispatcher.
- 0x59xxx x12 (599b4..59a38 + 58eac): SOUND-TEST HELPER.
  `bbs/bbc` on r8/r9 + `ld 0x59a90[r3*4]` table + immediates
  incl. `0x008e2950` (TEST stop #1). PUNCH variants + five
  stops + navigation live here.
- 0x3ae68/74 x2: record-field wrapper (`ld 0x14/0x18(r4)`,
  skip-if-zero) called by play_init (0x3a824, ORIGINAL symbol).
- 0x295e0, 0x2968c: FA-PLAYER corridor (0x29414 callees).
  `lda 0x00ad231f / 0x00ad2320, g0` + dedup (0x439ac) + filer.
  Same 0x00ad family as PUNCH (0x00ad1001).
- Immediate pairs `0x00ba7b00/0x00bb7b00` x8 across
  0xc7xx, 0xdexx, 0xf9xx, 0x3b1xx: same two-stinger shape in
  four nearby handlers (menu/transition).
- 0x00ad1xxx/0x00ad2xxx immediates: 0xb3c4 (`0x00ad1001`,
  THE punch value, boot/input region), 0xf7a8 (`...1006`),
  0xc73c (`...1007`), 0x1087c (`...1009`), 0x10294
  (`0x00ad2209`), 0x10864 (`0x00ad2200` + dedup), 0x44018
  (`0x00ad2101`).
- Stop family: 0x309f8/0x30a04 (`0x008e2e7f`,
  `0x009e2f7f`), 0x599bc (`0x008e2950`).
- Magic family `0x009e____` (-> Path F): 0x155a4, 0x227c4.
- Path-A magic `0x00ae101f`: 0xae28 (boot, +call 0x6d1ec).
- Table-driven: 0x2deac/0x2ded0 (tables 0x2aa88/0x2aa4c),
  0x47874 (table 0x478ec), 0x1af7c (play_motion, r3-fed).
- Record/loop-fed: 0x22c10 (loop `mov r5, g0` + dedup),
  0x18abc (mov r5 + dedup + filer).

## Ranking (trigger value)

1. 0x43xxx flusher -- only candidate that converts GAME
   STATE (flag bits) to commands at runtime. T4 prime target:
   find gameplay that sets a flushed bit (landed hit?).
2. 0x59xxx sound-test helper -- T2 replay target (known
   input path, known values).
3. fa-player sites -- fighter-task context needed; second
   gameplay lead.
4. Boot/init + menu stingers -- one-shot; bridge smoke only.

Next: T2 sound-test replay fixture (pre-gating snapshot +
`0x500704 = 0x10` -> `--until 0x43888` + memory trace).
