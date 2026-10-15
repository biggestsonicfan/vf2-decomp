# E1b: engine slot program recovered (9/10 arc)

Status: `sound_board_run_engine_key` + full-differential legs
green (engine, production, render, audible, bridge, live
round-1, a8c-hi, both synth legs). The dispatched voice now
programs a real SCSP slot + stream record in C. Full suite in
the commit validation.

## Recovery

Called from the sample-tail end (replacing the 0xc68 refuse)
with (voice, slot, d3, report): d2 via report->lookup_word
(both HIGH/LOW pickers store it). Head gates (table bit31 ->
0xed8, table +0x0c long != 0 -> 0xd92, both refused), then the
11-step program: SCSP[slot+0] long = 0x10300000|word(voice
+0x0c) as the model's u16 pair; stream[slot+0x0a] long = d5
- 0x200; +0x01 = 0x10; +0x1a/+0x1c = rate, rate+0xeba.
Refuses at 0xcb2 (handler pin): the copy loop is
dead-on-silicon. No step counting (board-level, no cpu).

## Bugs caught by the differential

- d6 is a LONG fetch (voice+0x0c, 4 bytes: 00 01 00 00),
  not a word: first draft wrote 0x10300001, silicon holds
  0x10310000 (SCSP reg[01]/[03] divergence).
- d2 pinning moved past the program's addi.w: S d2 at 0xcb2
  is rate+0xeba by construction (wrap exact); the report
  holds the pre-add rate.

## Ripple (all per-evidence, same slice)

Every dispatch leg's S run extends 0xc68 -> 0xcb2 (+18
steps uniformly: loopb-arm 322, a8c-hi 322, synth 112/103,
production 383, live round-1 407) with handler pins moved
to 0xcb2 and RAM/SCSP compares auto-expanded. No leg needed
a semantic change -- the engine code was already right;
only the stop points moved.

## Still open (engine-adjacent)

- 0xed8 / 0xd92 arms: refused, unmapped regions.
- Copy loop: dead-on-silicon, never stepped.
- Audible live voice still needs the key-on EFFECT path
  (slot_active in the model never sets from the program
  word alone -- render stays silent; S4b manual keying
  unchanged).
