# v0.0.24 texture orchestrator zero gates

## Scope

The observed texture-record loop repeatedly checks the 32-bit child state at
`0x00550080`. During the recorded startup path the value is zero for every
visit, selecting three deterministic control blocks:

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
`TEXTURE_LOOP_GATE`. Nonzero branches remain deliberately unsupported.

## Validation

CTest targets `vf2_orchestrator_gates` and `vf2_orchestrator_bridge` passed in a
warning-as-error build. The exact supported VF2 2.1 ROM set then produced:

```text
Native second-dispatch validation: MATCH
Final CPU and memory state:         MATCH
```

The three gate classes account for twelve of the 168 confirmed differential
checkpoints. Their function-catalog entries are promoted to
`dynamic-differential+unit`.

The confirmed bounded-bridge totals are 1,268,955 recovered and 1,867
interpreted instructions, with 266 recovered calls and 300 recovered returns.
