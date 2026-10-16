# Mapa de som — VF2.1 / Model 2A

> **Escopo:** endereços do modelo e dos caminhos medidos no repositório,
> não uma especificação exaustiva da placa física. Base:
> [sound_board.h](../../include/vf2/sound_board.h),
> [scsp.h](../../include/vf2/scsp.h),
> [ROM_LAYOUT](../ROM_LAYOUT.md) e notas citadas abaixo.
> Todos os endereços desta página são hexadecimais.

## Mapa visível ao Motorola 68000

| Base | Janela / região | Evidência e cuidado |
|---|---|---|
| `0x000000` | RAM de som, `0x080000` bytes no modelo | `VF2_SOUND_RAM_BYTES` |
| `0x100000` | Registradores SCSP, `0x1000` bytes modelados | 32 slots x `0x20`; bloco master em offset `0x0400` |
| `0x400000` | Latch/controle da placa | Não assumir todos os bits documentados |
| `0x600000` | ROM do programa 68000, `0x080000` bytes | Região `audiocpu`; bytes reorganizados por `vf2rom` |
| `0x800000` | Janela direta de amostras, `0x200000` bytes no modelo | A região `samples` reconstruída tem `0x800000` bytes; não inferir lógica completa de bancos |

A ROM de áudio do conjunto é identificada em
[decomp/m68k/README.md](../../decomp/m68k/README.md) como
`epr-17574.30`. O layout e os hashes exatos pertencem ao manifesto,
não a esta página.

## RAM de som (endereços **absolutos**)

| Endereço | Função observada / modelada |
|---|---|
| `0x1000` | Ring de comandos consumido pelo dispatcher, entradas de 4 bytes |
| `0x2500`–`0x2503` | Staging do receptor UART (página `$1500` relativa a A6) |
| `0x2504` | Cursor de escrita do ring (word) |
| `0x2506` | Contador compartilhado de entradas/recepção |
| `0x2508` | Cursor de leitura de comandos |
| `0x250e` | Comando corrente |
| `0x250f`–`0x2518` | Campos auxiliares de lookup e controle, conforme `sound_board.h` |
| `0x251e`, `0x252e` | Contadores de manutenção de vozes |
| `0x2526` | Estado da dança do receptor (`0x49e`) |
| `0x2800` | Tabela de 32 vozes x `0x10` bytes |
| `0x3000` | Descritores de stream, stride `0x20`; 6 descritores na API, 8 varridos na manutenção |
| `0x4000` | Descritores de comando, 20 x `0x10` bytes |
| `0x4200`, `0x4220`, `0x4240` | Tabelas de canal/controle |
| `0x5000` | Tabela/estágio de upload de amostras |

**Armadilha A6:** o programa 68000 frequentemente usa `a6 = 0x1000`.
Assim, `$1504(a6)` significa **RAM absoluta `0x2504`**,
`$1800(a6)` é `0x2800`, `$2000(a6)` é `0x3000` e
`$3000(a6)` é `0x4000`. A correção da montagem do ring para
`0x1000 + cursor` está em
[audio_8g_l3file_v0825.md](../../decomp/i960/notes/audio_8g_l3file_v0825.md).
O bloco `0x2000` **absoluto** também aparece como controle de upload
e **não** é o mesmo que `$2000(a6)`.

## Fila e transporte vistos pelo i960

| Endereço / PC i960 | Papel |
|---|---|
| `0x00504001` | Contador de entradas da fila |
| `0x00504002` | Cursor de leitura |
| `0x00504003` | Cursor de escrita |
| `0x00504010` | Palavra em estágio de transmissão |
| `0x00504014` | Índice de byte da palavra em estágio |
| `0x00504020` | Ring de 16 entradas, stride 4 |
| `0x00043888` | Filer / produtor da fila |
| `0x00043800` | Drainer / emissor de bytes |
| `0x01c80000` | Destino UART do drainer no conjunto estudado |

O antigo endereço `0x009c0000` em
[audio_8c_comms_hunt_v0825.md](../../decomp/i960/notes/audio_8c_comms_hunt_v0825.md)
foi **corrigido** pelo desmonte do drainer:
[audio_8f_drainer_v0825.md](../../decomp/i960/notes/audio_8f_drainer_v0825.md).
A ponte do projeto lê esse destino em
[src/app/session.c](../../src/app/session.c) e entrega o byte à API
`vf2_sound_board_file_uart_byte`. Não equivale a uma simulação completa
do 8251 ou de suas temporizações.

## Interrupções e processamento 68000

| Entrada ROM 68000 | Papel |
|---|---|
| `0x526` | L3: receptor de bytes UART e staging/ring |
| `0x5fe` | L1: manutenção/expiração de vozes e key-off |
| `0x1f16` | L2: tick de streams e programação associada |
| `0x7a6` | Drain/dispatcher de comandos de 4 bytes |
| `0x1f7c` | Interpretador de stream |
| `0xed8` | Braço de key-on recuperado |
| `0xf12` / `0xf50` | Entrada do allocator / saída de exaustão; reaper `0xf50` aberto |

A CPU 68000 usa **endereços de programa** para as entradas acima;
a ROM está **mapeada no barramento em `0x600000`**. Ao cruzar
`vf2m68k` com leituras de ROM, explicitar sempre se o número é
offset do programa ou endereço físico/barramento.

## Evidência, não extrapolação

- [v0805](../../decomp/i960/notes/audio_silicon_audit_scsp_confirmed_v0805.md):
  família de chip e offsets comprovados por instruções da ROM.
- [v0807](../../decomp/i960/notes/audio_fm_boundary_measured_v0807.md):
  a ROM não seleciona FM nos escritores reais encontrados.
- [E1c](../../decomp/i960/notes/audio_e1c_recovered_v0825.md):
  caminho de key-on concluído com diferencial.
- [E1f-d](../../decomp/i960/notes/audio_e1fd_lifecycle_v0825.md):
  cinco ticks de manutenção e expiração medidos.
- [E1o](../../decomp/i960/notes/audio_e1o_loopb_wiring_v0825.md):
  entradas do allocator e exaustão de vozes.

**Não documentado como fato:** registradores não escritos pela ROM,
bancos completos de amostra, temporização exata do SCSP, DSP,
DAC analógico e comportamento de hardware não exercitado pelos testes.
