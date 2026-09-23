# v0302: Fase 7 — taint `0x29414` + layout candidato do fighter

## Verdict

Os dois itens abertos da campanha `fa-player-v0293` estão fechados:

1. **Taint** em `0x29414` bit19 / path A–B medido com `tools/python/taint.py`.
2. **Layout candidato** consolidado em `include/vf2/fighter_candidate.h`
   com offsets de game_info (v0201), coli mid-body e corredor `0x29414`.

Nenhuma semântica C de gameplay foi inventada. Nenhum campo foi
renomeado para `health`/`damage`/`hitbox`.

## Taint `0x29414`

Parks/traces: `out/v297/trace-t6-b19-aa{10,11,21}.jsonl`,
`trace-t8-b19-aa11.jsonl` (gerados em v0297 via `vf2probe --set-ip`).

Bases (g7 do corredor): `fighter0 = 0x00510980`. Não usar
`0x00510800` como base primária — as janelas `0x2000` sobrepõem e o
offset vira `+0x324` em vez de `+0x1a4`.

Comando:

```sh
python tools/python/taint.py --rom-dir roms/vf2 \
  --vf2i960 build/Debug/vf2i960.exe \
  --scenario out/v302/29414-g7.json \
  --trace out/v297/trace-t6-b19-aa11.jsonl \
  --window 0x2000 --json out/v302/taint-29414-pathA.json
```

Resultado medido (idêntico em path A, path B, float tail e type 8):

```text
branch 0x0002949c depends on:
  fighter0 + 0x01a4 bit 19
  ; bbc 19, r14, 0x00029538
```

Ramos de janela (`0x294a4` `cmpobl 20,r12` e `0x294a8` `cmpobge 10,r12`)
aparecem **sem** taint no heurístico do `taint.py`: a carga `ldis` de
`+0x1aa` não propaga para o compare nessa implementação. A dependência
`+0x1aa` continua documentada pela trilha de memória e pela recuperação
v0297, não pelo taint.

Arquivos: `out/v302/taint-29414-{pathA,pathB,float,t8-pathA}.json`.

## Inferência de campos

### Corredor `0x29414` (g7 único)

| offset | largura | R/W | IPs | papel medido |
|--------|---------|-----|-----|--------------|
| `+0x0084` | 4B | R | `0x29538` | |
| `+0x017c` | 2B | RW | `0x294e8`, `0x294f0` | halfword add path A |
| `+0x018a` | 2B | RW | `0x294dc`, `0x294e4` | halfword add path A |
| `+0x01a4` | 4B | R | `0x29498` | flags; bit 19 via taint |
| `+0x01aa` | 2B | R | `0x294a0` | janela unsigned |
| `+0x01b1` | 1B | R | `0x29414` | type 0/6/8/10 |
| `+0x0614` | 2B | R | `0x29504` | path B mask `0x9000` |
| `+0x0c50` | 4B | W | `0x29544` | resultado float |

### Coli mid-body bilateral

Trace: `out/coli-midbody-v0289-full.jsonl`, bases
`0x00510980` / `0x00512980`.

| offset | largura | R/W | IPs |
|--------|---------|-----|-----|
| `+0x0004` | 1B | R | `0x22404` |
| `+0x01a4` | 4B | R | `0x2229c`, `0x222a0`, `0x2241c` |
| `+0x01a8` | 2B | R | `0x22410` |
| `+0x06dc` | 2B | W | `0x223b4` |
| `+0x0821` | 1B | R | `0x222a4` |

### Game_info (reconfirmado v0201)

`out/state8-case.jsonl`, `state8-case-bilateral.jsonl`,
`state4-case.jsonl` reproduzem exatamente os 10 offsets bilaterais de
`fighter_candidate_layout_v0201.md` (`base_count==2` em todos).

## Artefato

`include/vf2/fighter_candidate.h` — struct e macros estendidos;
`_Static_assert` por offset; nomes `field_XXXX` neutros.
`tests/recovered/test_fighter_candidate.c` cobre os offsets novos.

## Pins observados

- PUNCH **320/320 MATCH**, 12946 blocos, **14.962.620** insns, ambos `0x1645c`
- input-17 `--cycles 64` de `in17-c1`: **64/64 MATCH**, 2368 blocos, 2.428.988 insns
- ctest Debug **56/56**

## Fora de escopo / defer

- Renomear campos para semântica de hit/dano.
- Taint em coli contact real (`0x22404` bit8+scan) — park v0282 não
  reexecutado nesta fatia.
- Siblings `0x19ef8` — ainda bloqueados por `0x2704c` em replay.
