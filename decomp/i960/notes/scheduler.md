# Scheduler evidence

- Runtime-ready flag: `0x00500068[31]`.
- Natural setter observed at instruction `0x00009ca4`.
- Scheduler entry: `0x00010d54`.
- Runtime registry base: `0x00510000`.
- Per-task accounting base: `0x0050c000`, stride `0x20`.
- Descriptor stride is the stored stack-size field at `record + 0x08`.
- Runnable predicate proven for the initial pass: bit 31 of `record + 0x00`.
- Task entry is loaded from `record + 0x0c` and invoked indirectly.

Initial dispatch order:

1. `fa_game_info` — registry `0x00515200`, entry `0x0001645c`
2. `fa_camera` — registry `0x00515400`, entry `0x0001d320`
3. `fa_user` — registry `0x00515880`, entry `0x00029748`
4. `fa_sound` — registry `0x00515d80`, entry `0x000439fc`
5. `fa_kill_osage` — registry `0x00515e80`, entry `0x000657dc`
6. `fa_osage0` — registry `0x00515f00`, entry `0x000640f4`
7. `fa_osage1` — registry `0x00516180`, entry `0x000640f4`

The two osage descriptors intentionally share an entry point. Registry address
is therefore part of the dispatch identity. The complete task bodies and their
yield transitions are not yet accepted C recovery.
