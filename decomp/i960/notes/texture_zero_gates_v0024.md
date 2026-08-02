# v0.0.24 texture orchestrator zero gates

## Scope

The observed texture-record loop repeatedly checks the 32-bit child state at
`0x00550080`. During the recorded startup path the value is zero for every
visit, selecting three small deterministic control blocks:

| Entry | Observed action | Instructions | Visits |
|---|---|---:|---:|
| `0x0004bebc` | load state, take zero branch, call `0x0004cb64` | 3 | 4 |
| `0x0004bef4` | load state, take zero branch, call `0x0004cd18` | 3 | 4 |
| `0x0004bf2c` | load state and take loop-tail zero branch to `0x0004bf60` | 2 | 4 |

Across the observed path these gates recover 32 instructions and eight
procedure calls.

## C recoveries

`vf2_orchestrator_enter_zero_child_gate` accepts either child-call entry. It
reads the original state word into i960 `g0`, rejects any nonzero value, enters
the exact child procedure with the exact return address and accounts for three
instructions.

`vf2_orchestrator_apply_zero_loop_gate` handles the loop-tail branch. It reads
the same state word into `g0`, rejects nonzero values, advances the IP to
`0x0004bf60` and accounts for two instructions without changing procedure-frame
state.

`execute_texture_orchestrator_gate` maps these reports to the live hybrid bridge
kinds `TEXTURE_CHILD_GATE_A`, `TEXTURE_CHILD_GATE_B` and
`TEXTURE_LOOP_GATE`. The second-dispatch validator executes the same number of
reference interpreter instructions and compares complete CPU/memory snapshots
for every visit.

The nonzero branches remain deliberately unsupported.

## Test coverage

CTest targets `vf2_orchestrator_gates` and `vf2_orchestrator_bridge` verify:

- both child targets and return addresses;
- exact frame depth, call counter and instruction counter post-state;
- the two-instruction loop-tail transition;
- propagation of the state word into i960 `g0`;
- dispatch through the public hybrid bridge;
- rejection of nonzero state;
- rejection of invalid entry/frame state;
- invalid-argument handling.

The warning-as-error build and all ROM-independent tests passed before the
functional integration commit was written.

## Claim boundary

The three gate classes are integrated into the live hybrid bridge and account
for twelve of the 167 strict differential checkpoints. Combined with the
43-instruction inactive-record scan, they reduce the bounded bridge by 75
interpreted instructions.

The strict accounting is now:

- total bridge instructions: 1,270,822;
- recovered bridge instructions: 1,268,941;
- interpreted bridge instructions: 1,881;
- recovered bridge calls and returns: 266 / 300.

The remaining promotion condition is a local `native-second-dispatch` run with
the supported VF2 2.1 ROM set. Until that ROM-backed run reports `MATCH`, the
function catalog remains marked `pending-rom-differential+unit`.
