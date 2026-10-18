# v0864 — i960 queue → vector-15 → UART three-byte witness

Status: ROM-anchored opcode recovery and passive, fail-closed
transport observation. Not evidence of a naturally triggered
combat event, 68000 L3 reception or audible SCSP output.

## Exact original instruction anchors

The user-supplied Model 2A VF2.1 i960 ROM confirms 14 opcode
or immediate-address words:

| i960 PC | Observed contract |
| --- | --- |
| 0x000e24 | call 0x437d0 from vector-15 ISR |
| 0x4382c | aligned ring read, 0x504020 base + index |
| 0x43834 | stage complete word at 0x504010 |
| 0x4385c | load one selected byte from stage |
| 0x43868 | stores a 16-bit UART word at 0x01c80000 |
| 0x43870 | writes next byte cursor at 0x504014 |
| 0x43884 | returns from drain to ISR at 0xe28 |
| 0x43924 | original queue producer's aligned 32-bit store |

The six relevant MEMB extension / source-address words are
checked along with opcode words. The ISR serializes the
staged command **high usable byte first**, i.e. a 24-bit
command `0x00AD2342` becomes bytes `AD 23 42`.
The high-most `0x00` byte is not transmitted.

## New tool

`tools/python/audio_uart_transport.py` accepts the SAME trace
as the upstream v0861-63 selector, with optional
`--selector-chain`. It attributes a ring pop by **physical
slot, value and actual producer step**, requires the 32-bit stage
store at the ROM instruction, verifies byte reads in order 2,1,0,
then the little-endian UART stores and cursor writes (1,0,0xff)
in three **observed vector-15 services**, each returning from
the proper drainer RET. Slot overwrites invalidate the older
producer; partial stores never qualify. Missing steps, read,
stage, bytes, cursor, IRQ call or return refuse completion.

A generic queue→UART witness does **not** entail a character
selection. The stronger selector→UART counter is admitted
only when the prior same-trace v0863 selector witness proves
the exact writer step/slot/command triple. Neither counter
claims the Motorola 68000/SCSP was stepped.

Reproduce:

    VF2_ROM_ZIP=/path/to/vf2.zip python -m unittest discover -s tools/python -p 'test_audio_*.py'
    python tools/python/audio_uart_transport.py /path/to/vf2.zip out/real-precontact.jsonl --selector-chain --json

Local validation: 101/101 audio Python tests passed including
a real-ROM 14-word opcode assertion; 15 new ROM-free/ROM-backed
tests for the transport. Synthetic traces establish the
observer's acceptance/refusal behavior ONLY.

Next gate: natural pre-contact trace first; then UART bytes
correlated with 68000 L3→dispatcher→SCSP differential.
No C full suite or natural combat SFX was run or claimed here.
