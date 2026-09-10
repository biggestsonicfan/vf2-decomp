# fa_player drive bases (v0293)

Measured 2026-09-09 from parked snapshots. Reference executor only.

## Environment

- Build: Debug MSVC, `vf2_native_runtime_tests`, `vf2cycles`, `vf2probe`, `vf2i960`
- PUNCH pin: **320/320 MATCH**, 12946 blocks, **14.962.620** insns, both `0x1645c`
- ctest Debug: **56/56 passed** (158.99 s)

## Fighter bases (Work RAM)

Confirmed on `player-14288-rt` and `player-29414-sixth` via `--read-u32 0x00500804/08`:

| Slot | Address | Value |
|------|---------|-------|
| fighter0 (`0x500804`) | `0x00510800` | live |
| fighter1 (`0x500808`) | `0x00510980` | live |

Warm PUNCH/sixth parks use `0x510800` / `0x510980`, **not** `0x512800` / `0x512980`.
Always confirm before `--set-u32`.

## Snapshot park status

| Snapshot | Requested park | Actual IP | Status |
|----------|----------------|-----------|--------|
| `player-14288-rt.vf2snap` | `0x14288` / `0x19ef8` | `0x00019EF8` | OK |
| `player-29414-sixth.vf2snap` | `0x29414` | `0x00010F98` | **FAILED** (loop at ldob/cmpibe) |
| `player-29414-punch.vf2snap` | `0x29414` | `0x00010FA0` | **FAILED** (same loop) |

The `0x29414` park used 500000 steps from `sixth-regen`/`punch10`; the
reference executor never reached `0x29414` and the output snapshot was saved
at a wait loop around `0x10F98`. Re-park required with more steps or from a
later checkpoint.

## Fighter fields at park (fighter0)

| Offset | `player-14288-rt` | `player-29414-sixth` |
|--------|-------------------|----------------------|
| `+0x000` flags | `0` | `0` |
| `+0x1a4` state_flags | `0` | `0` |
| `+0x1b1` type byte | `0` | `0` |

Type `0` takes the already-recovered zero path of `0x29414`.
Non-zero path requires forcing `+0x1b1 ∈ {6, 8, 10}`.

## Global flags at park

| Address | Name | Value |
|---------|------|-------|
| `0x00500068` | runtime_flags | `0x80008000` |
| `0x00508000` | board_flags | `0x00008A00` |

- runtime bit 20 = clear (accepted `0x19ef8` guard is open)
- board bit 16 = clear
- board bit 5 = clear (`0x8A00` bit 5 is 0)

## `0x19ef8` fail-closed guards (`hybrid_execute_player_19ef8`)

Fails if any of:

- `player_state_flags (+0x1a4) != 0`
- `runtime_flags (0x500068) bit 20`
- `board_flags (0x508000) bit 16`
- `packed_value != 0x200`
- selector profile bytes wrong for `0x505` / `0x284`
- `player_flags` bits **5, 6, 21, 23**
- `selector` bits 13, 14
- `branch_byte` bit 6

Accepted path: 1652 insns (`0x505`) or 1805 (`0x284`), 4 calls / 4 returns,
exit `0x1428c`.

## `0x29414` zero-path C (`hybrid_execute_player_29414_zero_path`)

- Requires `cpu->ip == 0x00029414` and `g7 != 0`
- Type byte `+0x1b1 ∈ {6, 8, 10}` → `VF2_ERROR_UNSUPPORTED`
- Else stores `0` at `+0xc50`, sets `r14=0`, ip=`0x28178`, +7 insns, return

## Next

- Phase 1: flag-bit siblings from `player-14288-rt` (park is valid)
- Phase 2: re-park `0x29414` with larger `--max-steps` or from
  `player-14288-rt` after the accepted `0x19ef8` path
