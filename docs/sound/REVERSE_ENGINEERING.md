# Investigação e validação do áudio — procedimento reproduzível

> **Runbook proposto** para a próxima etapa; não foi executado por esta
> atualização de documentação. Respeitar [AGENTS.md](../../AGENTS.md):
> nenhum ROM/dump/áudio proprietário no Git, observadores passivos,
> caminhos desconhecidos retornam `VF2_ERROR_UNSUPPORTED` e
> comportamento recuperado só entra em C após diferencial com o oráculo.

## 1. Preparar o ambiente e conferir o conjunto

No Windows/MSVC (fluxo canônico do repositório):

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
build\Debug\vf2rom.exe verify roms/vf2
build\Debug\vf2m68k.exe info roms/vf2
build\Debug\vf2m68k.exe disasm roms/vf2 0x526 32
build\Debug\vf2m68k.exe disasm roms/vf2 0xed8 32
```

`roms/vf2` é um **exemplo de caminho local** para uma cópia legal.
Os comandos são derivados do [README](../../README.md) e das notas
de recuperação; **não foram testados nesta publicação**.
Em particular, conferir as opções disponíveis com `--help` antes
de automatizar scripts novos.

Inspeção de instruções: [OPCODE_ATLAS.md](OPCODE_ATLAS.md) e
[OPCODE_TRACE.md](OPCODE_TRACE.md). Agora o `vf2m68k disasm` aceita
`--raw` no final para mostrar a palavra de opcode e bytes de extensão,
sem alterar o stepper.

## 2. Escolher um evento de gameplay, não um pacote inventado

A [composição 8h](../../decomp/i960/notes/audio_8h_bridge_v0825.md)
prova que um pacote *escolhido no teste* percorre fila -> UART
simulada -> ring -> voz -> WAV, mas isso **não** identifica um
evento real que emita o pacote. O
[fechamento T5](../../decomp/i960/notes/audio_t5_closeout_v0825.md)
mostra que a fila de boot tem tráfego e o pump drena os bytes,
mas que o contexto de gameplay ainda não produz uma voz audível
em sessão.

Experimento sugerido:

1. Preparar **snapshots locais** antes de um evento de contato real,
   transição de round ou comando de SOUND TEST (não versioná-los).
2. Instrumentar, passivamente, a escrita na fila
   `0x504020` e o contador `0x504001`; registrar PC i960,
   valor, ordem e estado do frame.
3. Capturar a entrada/saída de `0x43888` e `0x43800` e
   cada byte enviado a `0x01c80000`.
4. Reproduzir os mesmos bytes na entrada L3 `0x526`, observar
   `0x2504/0x2506/0x2508`, os quatro bytes do ring e o
   PC de despacho 68000.
5. Seguir alterações em `0x2800` (vozes) e em
   `0x100000` (SCSP), então comparar o áudio.

A ausência de escrita numa janela estacionada não prova que o
jogo nunca emite áudio: a nota
[8c](../../decomp/i960/notes/audio_8c_comms_hunt_v0825.md)
documenta justamente essa armadilha com probes sem avanço de
interrupção de frame. Usar um **driver de frames** e um
pré-evento válido.

## 3. Construir um golden trace mínimo

Esquema de log **proposto**, não formato atual de ferramenta:

```text
run_id,frame,side,cpu,pc,event,address,value,queue_count,ring_count,voice_slot,step
```

Usar linhas separadas para `queue_write`, `uart_write`,
`l3_file`, `ring_commit`, `dispatch`, `scsp_write`,
`keyon`, `render` e `irq`. Campos desconhecidos ficam
vazios (não inventar). Para comparar duas execuções, ordenar
por evento/step e reportar o **primeiro desvio**, com:

- endereço e tamanho do acesso, endianess e valores antes/depois;
- PC de origem, registradores relevantes e classe do comando;
- RAM de som e SCSP antes/depois no limite diferencial;
- contador/cursor e eventuais exclusões **justificadas**;
- estado do renderer e hash de PCM, quando aplicável.

Evitar publicar o trace integral: registrar somente métricas,
hashes e pequenos fatos não reconstrutivos. Guardar ROM, snapshot,
WAV e traces brutos apenas no ambiente local.

## 4. Comparar o C com o executor 68000

O repositório **já possui** o executor
[src/m68k/stepper.c](../../src/m68k/stepper.c)
(`vf2_m68k_run`) e testes de diferencial; não é necessário
esperar por um emulador novo para testar o fluxo lógico de
comandos. A ordem de prova é:

```text
snapshot PRÉ-evento
  -> executar C a partir da cópia A
  -> executar stepper 68000 a partir da cópia B
  -> parar no mesmo limite de instrução/handler
  -> comparar RAM + registradores + SCSP + eventos
  -> corrigir primeiro desvio ou recusar explicitamente
```

Usar os precedentes [E1c](../../decomp/i960/notes/audio_e1c_recovered_v0825.md),
[E1f-d](../../decomp/i960/notes/audio_e1fd_lifecycle_v0825.md) e
[E1o](../../decomp/i960/notes/audio_e1o_loopb_wiring_v0825.md).
O render PCM **não** dispõe do mesmo oráculo físico de amostras:
determinismo de WAV sintético demonstra consistência do modelo,
não fidelidade da placa.

## 5. Ordem recomendada de trabalho

| Prioridade | Experimento | Critério de saída |
|---|---|---|
| P0 | Capturar primeiro filer em contexto real de gameplay | Snapshot pré-evento + PC + payload + replay |
| P1 | Validar L2 e L1 em frames sucessivos com o pump | Timeline de ticks e ring/vozes sem staging artificial |
| P2 | Recuperar `0xf50` reaper, com entradas de 32 vozes ocupadas | C-vs-stepper e recusas preservadas |
| P3 | Medir bancos/amostras/loop/pitch/envelope | Mapa de escritores e comparação PCM de referência |
| P4 | Reconciliar registradores e documentação externa Model 2A | Matriz por endereço: ROM, MAME, manual, confiança |

## Fontes e ferramentas

- [README](../../README.md): CLI e build.
- [decomp/m68k/README](../../decomp/m68k/README.md): mapa de
  handlers 68000 e cobertura atual.
- [docs/NATIVE_DIFFERENTIAL](../NATIVE_DIFFERENTIAL.md):
  contrato do diferencial i960 (usar como disciplina, sem
  pressupor que cobre automaticamente todos os periféricos).
- [src/app/session.c](../../src/app/session.c):
  `vf2_session_pump_audio`, drainer no shadow i960 e
  classificação de despachos.
- [tests/hardware/test_sound_board.c](../../tests/hardware/test_sound_board.c)
  e [tests/host/test_session_audio.c](../../tests/host/test_session_audio.c).
- [MAME Model 2](https://github.com/mamedev/mame/blob/master/src/mame/sega/model2.cpp)
  e [MAME SCSP](https://github.com/mamedev/mame/blob/master/src/devices/sound/scsp.cpp):
  referência de hardware auxiliar, sem promover suas
  aproximações a oráculo do VF2.1.

**Escopo desta publicação:** documentação apenas; nenhuma alteração de
implementação, testes ou ROMs.
