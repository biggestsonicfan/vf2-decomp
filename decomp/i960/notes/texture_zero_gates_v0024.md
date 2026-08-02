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

Across the observed path these gates account for 32 instructions and eight
procedure calls.

## C recovery candidates

`vf2_orchestrator_enter_zero_child_gate` accepts either child-call entry. It
reads the original state word into i960 `g0`, rejects any nonzero value, enters
the exact child procedure with the exact return address and accounts for three
instructions.

`vf2_orchestrator_apply_zero_loop_gate` handles the loop-tail branch. It reads
the same state word into `g0`, rejects nonzero values, advances the IP to
`0x0004bf60` and accounts for two instructions without changing procedure-frame
state.

The nonzero branches remain deliberately unsupported.

## Unit coverage

CTest target `vf2_orchestrator_gates` verifies:

- both child targets and return addresses;
- exact frame depth, call counter and instruction counter post-state;
- the two-instruction loop-tail transition;
- propagation of the state word into i960 `g0`;
- rejection of nonzero state;
- rejection of invalid entry/frame state;
- invalid-argument handling.

## Claim boundary

These functions are semantic candidates and are not yet dispatched by the live
hybrid bridge. The strict validator therefore remains at 1,268,866 recovered
and 1,956 interpreted instructions.

The inactive-record scan and these three gate classes together represent 75
additional observed instructions. If all candidates pass live CPU/memory
differential integration, the bounded bridge would move to 1,268,941 recovered
and 1,881 interpreted instructions.
