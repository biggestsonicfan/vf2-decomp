# `fa_game_info` `0x18644`: positive state-8 bit-6 compositions

The positive-threshold state-8 bit-6 scan identified two small compositions
whose ROM paths remain in the already recovered `0x188ac -> 0x188cc` tail:

- `0x140`: state bit 8 plus fighter flag bit 6;
- `0x142`: state bit 8 plus fighter flag bits 1 and 6.

The values are the measured `fighter + 0x1a4` fields. Each composition was
validated in all three physical distributions (`value/0`, `0/value` and
`value/value`), both countdown values and both mode-byte-bit-6 settings: 12
strict cases per composition.

For `0x140`, the bilateral state was previously rejected by the native
admission guard. After admitting that exact pair, the ROM/native instruction
counter differences showed a call-order-specific rejoin. The native child now
models the measured first/second-call corrections for both countdown states
and both mode settings; CPU state, condition state, mutable memory and all
serialized counters match at the scheduler boundary.

For `0x142`, the existing positive admission covered the path but its
no-countdown accounting was short. Per-call comparison showed the isolated
first-order, isolated second-order and bilateral rejoin deltas separately;
those exact corrections are now applied only to the measured `0x142`
compositions. All 12 cases match the ROM at the final scheduler boundary.

The neighboring positive bit-6 compositions, including state-8 + bit-2/bit-6
and state-8 + bit-4/bit-6 families, remain unsupported or unproven and are not
admitted by this note.
