# v0.2.0 texture orchestrator limits full branch at 0x0004bfe0

## Recovered region

The i960 cluster at `0x0004bfe0` selects texture limits written to `0x00550004`/`0x00550008`.
Reference disassembly (`build/Debug/vf2i960 disasm D:/ia/vf2-decomp/roms/vf2 0x0004bfe0 80`) shows:

```
0004bfe0 ld 0x00500068,r15; bbs 16,r15,0x4c0ec
0004bfec ldob 0x0050002b,r15; lda 0xc,r14; cmpobe r14,r15,0x4c0b4
0004c000 lda 0xd,r14; cmpobe r14,r15,0x4c0b4
0004c00c ldob 0x0050002b,r15; lda 0xc0,r14; bbs r15,r14,0x4c0fc
0004c020 lda 0xc000,r14; bbs r15,r14,0x4c0d8
0004c02c lda 0xc,r14; bbs r15,r14,0x4c11c
0004c038 lda 9,r14; cmpobne r15,r14,0x4c068
0004c044 ldob 0x00500064,r14; lda 6,r15; cmpobe r14,r15,0x4c0c4
0004c058 lda 8,r15; cmpobe r14,r15,0x4c0c4
0004c064 b 0x4c07c
0004c068 lda 0x3e80,r10; lda 0x4e20,r11; b 0x4c10c
0004c07c ldob 0x00500031,r15; lda 8,r14; cmpobl r15,r14,0x4c0a4
0004c090 lda 0x3e80,r10; lda 0x4e20,r11; b 0x4c10c
0004c0a4 lda 0x4330,r10; mov 0,r11; b 0x4c10c
0004c0b4 mov 0,r10; lda 0x4e20,r11; b 0x4c10c
0004c0c4 lda 0x4330,r10; lda 0x4e20,r11; b 0x4c10c
0004c0d8 lda 0x32c8,r10; lda 0x4e20,r11; b 0x4c10c
0004c0ec mov 0,r10; lda 0x4e20,r11; b 0x4c10c
0004c0fc lda 0x12a8,r10; lda 0x4330,r11
0004c10c st r10,0x00550004; st r11,0x00550008
0004c11c ret
```

`bbs r15,r14` tests bit `r15 % 32` of `r14` (mask as source). Thus:

* `0x00500068` bit 16 set -> `0/0x4e20` (8 instructions to `0x4bd00` via ret).
* `0x0050002b == 12` or `13` -> `0/0x4e20` (11/13 instructions).
* `display_mode % 32 in {6,7}` -> `0x12a8/0x4330` (15 instructions) via `0xc0` bits 6,7.
* `display_mode % 32 in {14,15}` -> `0x32c8/0x4e20` (18 instructions) via `0xc000` bits 14,15.
* `display_mode % 32 in {2,3}` -> branch to `ret` at `0x4c11c` skipping both stores (15 instructions, sentinel unchanged) -> `VF2_ERROR_UNSUPPORTED`.
* `display_mode == 9` -> secondary dispatch on `0x00500064` (`6`/`8` => `0x4330/0x4e20`, 25 instructions) else `0x00500031 < 8` => `0x4330/0` (31 instructions) else `0x3e80/0x4e20` (31 instructions).
* otherwise -> `0x3e80/0x4e20` (22 instructions).

The six supported limit pairs are therefore:

| lower | upper | condition |
|-------|-------|-----------|
| `0x3e80` | `0x4e20` | default (205 of 256 modes with `runtime bit16` clear and not in other classes) |
| `0x4330` | `0x0000` | `mode==9` and `0x50064∉{6,8}` and `0x50031<8` |
| `0x0000` | `0x4e20` | `runtime bit16` or `mode∈{12,13}` |
| `0x4330` | `0x4e20` | `mode==9` and `0x50064∈{6,8}` |
| `0x12a8` | `0x4330` | `mode%32∈{6,7}` |
| `0x32c8` | `0x4e20` | `mode%32∈{14,15}` |

## ROM-backed differential evidence

Synthetic snapshots at `0x0004bfe0` were created via `make_orchestrator_snap` (`vf2_i960_cpu_enter_procedure` with `0x4bfe0 -> 0x4c11c`) and swept with `vf2probe`:

```
build/Debug/vf2probe --rom-dir D:/ia/vf2-decomp/roms/vf2 \
  --snapshot snap_orch.vf2snap --set-u8 0x0050002b=<mode> \
  --set-u32 0x00500068=<flags> --set-u8 0x00500064=<v64> \
  --set-u8 0x00500031=<v31> --set-u32 0x00550004=0x11111111 \
  --set-u32 0x00550008=0x22222222 --until 0x0004c11c \
  --read-u32 0x00550004 --read-u32 0x00550008
```

Sweep `display_mode 0..255 × runtime_flags {0,0x10000}` with secondary fields zero (512 cases) matches the C implementation exactly:

* 258 cases `0/0x4e20` (256 with `runtime bit16` + 2 with `mode 12/13`),
* 16 cases `0x12a8/0x4330`,
* 16 cases `0x32c8/0x4e20`,
* 1 case `0x4330/0` (`mode 9` with `0x50064=0,<8`),
* 205 cases `0x3e80/0x4e20`,
* 16 cases `2,3 mod32` -> `VF2_ERROR_UNSUPPORTED` (skip stores, sentinel unchanged, `0x11111111/0x22222222` retained).

Mode 9 extra-field sweep (`0x50064 ∈{0,6,8}` × `0x50031 ∈{0,7,8,255}`) also matches (24 instructions for `6/8`, 30 for the `<8` versus `>=8` split). `vf2_orchestrator_limits_tests` locks all 512 plus the extra-field matrix.

## Recovered C

* `src/analysis/orchestrator_limits.c`: `vf2_orchestrator_select_limits` implements the full dispatch (mask-as-source `bbs` semantics, `mode%32` classes, `9` secondary), `vf2_orchestrator_select_default_limits` is the `0,0` wrapper, `vf2_orchestrator_apply_default_limits` reads `0x50068/0x5002b/0x50064/0x50031`, writes `0x55004/0x55008` and reports the measured instruction equivalents (8/11/13/15/18/22/25/31).
* `include/vf2/analysis/orchestrator_limits.h`: exposes `VF2_ORCHESTRATOR_FIELD_50064/50031`, limit constants and the new `vf2_orchestrator_select_limits`.

The hybrid bridge at `0x0004bfe0` now recovers the full cluster without remaining `UNSUPPORTED` for the six pairs; only the `2,3 mod32` skip remains fail-closed by design (no store).
