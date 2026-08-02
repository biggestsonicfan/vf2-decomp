# v0.0.22 gameplay/geometry helper evidence

## Accepted procedures

- `0x00009444`: inline diagnostic thunk entered through `balx`; copies the
  inline string, advances the tile destination by 128 bytes, scans aligned
  inline words through the terminator and resumes at the embedded continuation.
- `0x0004d2c0`: texture-status line; writes `TEX` or `t4e` and an indexed
  texture name to the observed tilemap row.
- `0x0000281c`: game-state classifier; all eleven live calls use a non-25 mode,
  clear flag bits 0/1 and a zero table mapping.
- `0x000026ec`: color/control lookup; eight live calls use selector 0 or 1,
  invoke the accepted classifier path and add `0x00010101` to the table value.

## Differential totals

```text
bridge instructions:          1270822
recovered instructions:       1268752
interpreted instructions:        2070
recovered blocks/checkpoints:  143/143
recovered calls/returns:       250/297
final state:                   MATCH
```

## Claim boundary

Alternate classifier flags, mode 25 and nonzero classifier mappings are not
claimed. The recovered functions return `VF2_ERROR_UNSUPPORTED` for those
states. The remaining interpreted code is concentrated in the top-level
texture orchestrator and later geometry preparation.
