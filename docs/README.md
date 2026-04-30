# Procedimento de upload e captura de saída

Os 3 modos do POC compilam (ver `_esp32/versao.txt`). Esta pasta guarda
as saídas reais capturadas no terminal via `pio device monitor` para os
critérios de aceitação 1, 2 e 13 do plano.

## Pré-requisitos

- ESP32 DevKit V1 conectado via USB.
- Driver USB-Serial instalado (CP2102 ou CH340 conforme a placa).
- Modo selecionado em `platformio.ini` (descomente a linha desejada de
  `build_flags`).

## Procedimento (modo `BUILD_ANALISE` — sem hardware extra)

```bash
cd _esp32

pio run                  # compila firmware
pio run -t upload        # grava firmware no ESP32
pio run -t uploadfs      # grava data/ (KMLs) no LittleFS
pio device monitor       # abre o terminal serial em 115200 baud
```

A primeira execução de `pio device monitor` mostra a saída do `setup()`
do firmware: sumário do KML + CSV de auditoria + 9 pontos demo.

### Como capturar a saída

A maneira mais simples no VSCode:

1. Espera o `pio device monitor` imprimir até `=== FIM ===`.
2. Clica no terminal e seleciona toda a saída relevante (do
   `=== POC VRA_Controlador ===` até `=== FIM ===`).
3. Copia (`Ctrl+C` na IDE / `Ctrl+Shift+C` em alguns terminais).
4. Cola num arquivo novo: `_esp32/docs/sample_output_analise.txt`.
5. Salva.
6. Para sair do monitor: `Ctrl+C` no terminal (interrompe o pio).

## Procedimento (modo `BUILD_TESTS`)

Igual ao acima, trocando o modo no `platformio.ini` para `-DBUILD_TESTS`.
A saída esperada são 9 linhas com `[PASS]` ou `[FAIL]` + tempo de
execução em µs/op, mais o resumo final `=== RESUMO: X PASS, Y FAIL ===`
e o bench de latência (critério: média ≤ 5000 µs).

Salvar em `_esp32/docs/sample_output_tests.txt`.

## Procedimento (modo `BUILD_POC`)

Requer hardware adicional: GPS UBlox M8N em UART2, atuador linear DC com
PWM bidir + feedback ADC potenciométrico, botões físicos START/STOP.
**Opcional para v0.1** — o critério 3 do plano só exige que compile,
não que tenha hardware completo.

Comandos via terminal (linha + Enter):

- `m e`  — entra em modo execução (esperando START + GPS válido)
- `m c1` — calibração 1 (extremos do potenciômetro)
- `m c2` — calibração 2 (tabela posição → abertura)
- `m c3` — calibração 3 (fator de escoamento)

Salvar em `_esp32/docs/sample_output_poc.txt`.

## Notas

- `pio device monitor` no Windows pode prender a porta. Se aparecer
  "could not open port", feche outros terminais que estejam usando
  COM<n> e tente de novo.
- Para mudar a baud-rate ou a porta, edite `platformio.ini`
  (`monitor_speed`, `monitor_port`).
- Se o ESP32 entrar em loop de reset (panic), o `monitor_filters =
  esp32_exception_decoder` traduz o stack-trace em nomes de função —
  útil para depurar.
