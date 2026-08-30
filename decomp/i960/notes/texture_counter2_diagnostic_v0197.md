# v0197: counter2 out-of-range texture diagnostic

The third texture counter (`0x005502e0`) dispatches through helper `0x0004b44c`. Its first nested operation is the same record publisher at `0x0004b9b8` recovered in v0196, so texture numbers greater than `0x56` must take the same `tex num error` diagnostic path instead of failing before the helper is entered.

The recovered counter2 path now lets the publisher handle the texture-number range. For an out-of-range value it renders the v0196 diagnostic, skips the `0x00550288` record publication, then continues through the queue helper at `0x0004ba70` exactly as the i960 does. The measured counter-update corridor consumes 198 instructions and five procedure calls/five returns for this case.

## Validation

Full snapshots were compared from `0x0004bb98` through `0x0004bc58` for counter2 expiry.

- seven values spanning valid controls, `0x56/0x57`, large positive values, wraparound-negative values, and `INT32_MIN`;
- `argument1` values `0`, `1`, and `0x10`;
- **21/21 exact ROM-backed snapshots**;
- standard local CTest gate: **23/23**;
- ROM-backed `vf2_texture_bridge_differential`: **passed**.

No ROM content is committed.
