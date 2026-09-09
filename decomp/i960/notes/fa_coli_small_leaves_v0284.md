# fa_coli small leaves `0x233d0` + `0x2364c` — v0284

Native C recovery of the two remaining small warm-path callees inside
`0x23524`, measured in v0283.

## `0x233d0` flag builder

Late entry reloads both fighter pointers from Work RAM `0x500804` /
`0x500808`, loads `+0x1a4` from each, builds `or`/`and`/`xor`, clears
`g6`, and branches backwards into the shared body at `0x23398`.

Warm PUNCH taken path (all siblings fail-closed):

- neither fighter `+0x1a8` (ldos) matches `{0x242, 0x241, 27<<2}`
- `or` bit 18 clear, `xor` bit 8 clear, `and` bit 8 clear, `or` bit 14 clear
- `g6` bits 0/1/2 stay clear (`bbs 0` / `bbs 2` / `bbc 1` fall through)
- both `+0x1a4` bit 16 clear
- copies six words from ROM `0x232c4` into `g13+0xb4, +0xb8, +0xc0,
  +0xc4, +0xbc, +0x88`
- writes `g6 = 0` (survives `ret`)
- **44 instructions / 0 nested calls / 1 return**

Measured ROM row: `0, 0, 0x3f, 0x3f, 0x0002336c, 0`.

## `0x2364c` FIFO delta push

Caller `0x23694` already reloaded `g7`/`g8`. The leaf loads both
fighters' `+0x1f4` triples, computes float `subr` deltas, and pushes
them through the polygon FIFO.

Warm PUNCH taken path:

- both `+0x1f4` triples are all-zero → deltas 0, 0, 0
- writes `0x18003030` (command) plus three zero words to FIFO `0x884000`
- reads three words back from the same FIFO
- stores the three replies at `g13+0xc8, +0xcc, +0xd0`
- **17 instructions / 0 nested calls / 1 return**

Non-zero `+0x1f4` fails closed (unmeasured). FIFO replies are read from
hardware, not hardcoded.

## Hybrid parent

`hybrid_execute_coli_body` now:

1. after the six `0x23878` walk, interprets `window_entry → 0x233d0`,
   runs `vf2_hybrid_coli_233d0_execute`, interprets `0x235a0 → 0x238a4`;
2. after `0x238f8`, interprets `0x23644 → 0x2364c`, runs
   `vf2_hybrid_coli_2364c_execute`, interprets `0x236b8 → 0x22210`.

## Validation

- unit tests `test_coli_233d0_flag_builder`, `test_coli_2364c_fifo_delta`
  (warm poststate + fail-closed siblings)
- `ctest --test-dir build -C Debug` — all tests
- PUNCH ROM-backed: `320/320` MATCH, `12946` blocks,
  `14,962,620` instructions, both sides at `0x1645c`
- whole-task pin unchanged (`9214 / 18 / 19` is embedded in that corridor)

## Remaining interpreted inside `0x23524`

`0x2396c` (5236, ×2) and the 179-insn shell.
