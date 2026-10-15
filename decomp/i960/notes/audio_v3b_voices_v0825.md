# V3b: voice-record naming (v0825)

Status: 12 evidence-backed field constants in sound_board.h +
mechanical rename across sound_board.c (64 sites, zero
leftovers). Behavior-identical: board + cdispatch + triangle +
plumbing gates all green post-rename; full suite in the commit
validation. Tests' own staging numbers untouched (fixtures,
not recovery).

## Layout (16 B records, stride 0x10, 32 voices at 0x2800)

| off | name | grounding (>=2 recoveries each) |
|-----|------|----------------------------------|
| 0x00 | SLOT (u16) | V2 slot=index*0x20; S3 odd-guard + SCSP[slot] stores; S4 SCSP[slot+8] |
| 0x02 | FLAGS | V1 b0 active gate; b3 release-path select; V1 key `\|=0x10`; V5 b6 channel gate + `=0` teardown |
| 0x03 | BYTE | $150d/$1515 filed byte; picker nibbles; 0xba sentinel (9ba scan) |
| 0x04 | CHANNEL | authored both branches; V5 compare gate; teardown-cleared |
| 0x05 | AGE_RELEASE | release-arm rank ($152e cascade); seeded $152e/5; L1-ticked; cleared |
| 0x06 | SELECT (u16) | $1516 word; `&0x7ff * 20` sample-table index (S4 tail) |
| 0x08 | MODE | $1510 filed both branches; volume-math input |
| 0x09 | AGE | normal-arm rank ($151e cascade) + 0xff release eligibility; filed $151e |
| 0x0a | META | $1518 filed (LOW); picker addend (voice+desc[5]) |
| 0x0b | RATE | rate-table seed (0x600c48); L1 countdown to key-off |
| 0x0c | SAMPLE (u32) | V2 sample longs from the 0x600726 table |

Bytes 0x07/0x0d-0x0f: untouched by every committed recovery --
no constant (honest gap, not padding claimed).

## Naming discipline (AGENTS.md trap compliance)

Role-names only where BEHAVIOR shows the role (FLAGS bits are
tested; CHANNEL is compared; RATE is seeded from the rate
table AND ticked; AGE ranks cascades). Filed bytes take the
lookup vocabulary (BYTE/MODE/META match LOOKUP_*). Nothing is
named health/state/pose -- those semantics remain unproven.

## 8/10 status after V3b

Completable tranches done: attach, determinism, ring, WAV
pipe, receiver map, driver, hunt negatives, naming. The bar's
remaining item is live packets (bridge), blocked on a staged
writer fixture (landed-hit setup or round-boundary play).
Nothing else in 8/10 is actionable without it; claiming the
bar now would launder the missing bridge. 9/10 (engine, B0/C0
heads, LOW row-table, compactor) is untouched.
",
