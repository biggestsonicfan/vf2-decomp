# Texture counter status-word v0198

The `execute_texture_counter_update` helper at `0x0004bb98` dispatches
`0x005502e0 == 1` through `0x0004b44c`. The previous C guard read
`0x0055c2f0` (VF2_TEXTURE_STATUS_WORD) and returned
`VF2_ERROR_UNSUPPORTED` when the half-word was `>= 1`. No live context
supported that guard.

Measured with the ROM from the sixth-dispatch snapshot
`out/sixth.vf2snap -> out/tex_counter_boundary.vf2snap` (`0x0004bb98`):

- reference `vf2probe` from `0x0004bb98` to `0x0004bc58` was traced with
  the three counters at `0x005502c0/0x005502d0/0x005502e0` set to the 27
  Cartesian values `{0,1,2}` (2 represents `>1`; decremented form was
  checked for `5`) and with `*0x0055c2f0` in `{0,1,0xFFFF}`:

```
c0 c1 c2 -> run_instructions / calls
0 0 0 12, 0 0 1 42, 0 0 2 14,
0 1 0 90, 0 1 1 120, 0 1 2 92,
...
1 1 1 172  (max) etc.
```

- setting `*(u16*)0x0055c2f0 = 1` (and `0xFFFF`) with `c2=1` still
  reaches `0x0004bc58` with `42` instructions and `3` calls — the same
  `0x0004b44c` helper path as `0x0055c2f0 == 0`. Memory tracing shows
  no access to `0x0055c2f0` on that corridor; the half-word is stale
  from an earlier texture record and is not consulted by the i960 for
  this dispatch. Out-of-range texture numbers (`> 0x56`) were also
  probed for `c0==1` (`0x005502c4=0x57`): reference reaches `0x0004bc58`
  in `376` instructions (`3` calls) — the double-diagnostic path via
  `0x0004b9b8` (`tex num error`, 160 instr each, 19 cells).

- the previous guard therefore caused the native bridge to fail closed
  on a path the oracle treats as ordinary state. The reference never
  branches on `0x0055c2f0` here.

Fix: remove the `0x0055c2f0 >= 1` early-return in
`src/recovered/texture_bridge_texture.c:execute_texture_counter_update`
(the `read_u16` + `VF2_ERROR_UNSUPPORTED` guard). The half-word is kept
as `(void)status_word` to preserve the local, but it no longer blocks
the dispatch. The `0x0004b44c` helper is entered regardless of the
half-word, matching the i960.

Validation:
- `vf2_texture_bridge_differential` still MATCH (1,270,824/0, 190 checkpoints).
- `ctest -C Debug` 52/52 passed.
- targeted `vf2probe` from `tex_counter_boundary.vf2snap` for the 27
  Cartesian counter combos × 3 status-word values (81 cases) all reach
  `0x0004bc58` in the recovered native bridge with the exact
  `run_instructions`/`calls`/`writes` measured above; the `c2==1` +
  `status_word=1/0xFFFF` cases now match instead of failing closed.
