# fa_game_info 0x18644: exact 0x104 ↔ 0x114 recovery (v0044)

The ordered pair family `0x104 ↔ 0x114` is now ROM-backed and exact for the recovered native child at `0x00018644`.

States:

- `0x104 = bit8 | bit2`
- `0x114 = bit8 | bit2 | bit4`
- vector class: 1 (`197,202,194,199`)

Semantic recovery admitted only this exact bilateral composition in the general bilateral gate and in the non-bit1 threshold gate.

Before accounting, architecture/calls/returns/interrupt counters were already exact for all 16 isolated probes. Native-minus-ROM instruction deltas were:

- `0x104 -> 0x114`, first call: `+7` without countdown, `-4` with countdown;
- `0x114 -> 0x104`, first call: `+10` without countdown, `-4` with countdown;
- starting forward, second call: `+14/+13` without countdown for mode6 `0/1`, `-3` with countdown;
- starting reverse, second call: `+11/+10` without countdown for mode6 `0/1`, `-3` with countdown.

As in other bilateral recoveries, the second native call observes the fighters in the opposite local role, so the no-countdown second-call corrections are intentionally role-swapped between the local forward/reverse predicates.

Validation against ROM after accounting:

- isolated call-site differential: 16/16 exact;
- chained `C child #1 → caller ROM → C child #2 → tail ROM`: 8/8 exact;
- architectural snapshot: exact;
- instructions, calls, returns, interrupt entries and interrupt returns: all zero delta.

ROM-validated functional candidate: `43f922114d28635bddafaef181cae25d33132f23`.

After this recovery the enumerated matrix has 32/64 ordered pairs individually marked exact, with all 9 instruction-vector classes represented.
