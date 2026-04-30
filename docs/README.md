# Procedimento de upload e captura de saída

Procedimento de compilação, upload e captura de saída para os 4 modos
do firmware. Esta pasta guarda as saídas reais capturadas no terminal
via `pio device monitor` (`sample_output_*.txt`).

## Pré-requisitos

- ESP32 DevKit V1 conectado via USB.
- Driver USB-Serial instalado (CP2102 ou CH340 conforme a placa).
- Modo selecionado em `platformio.ini` (descomente a linha desejada de
  `build_flags`).

## Procedimento (modo `BUILD_SIM` — default, sem hardware extra)

Modo plug-and-play: basta um ESP32 DevKit V1 + cabo USB. No boot, o
firmware lista os `*.kml` encontrados no LittleFS e pede para escolher
via Serial. Roda a simulação completa (trajetória boustrofédon com
velocidade variável + Lógica Hierárquica + PID + atuador simulado) e
imprime relatório final estilo `coverage_report.py` do Python.

```bash
cd _esp32
pio run                  # compila firmware (default: BUILD_SIM + LANG_PT)
pio run -t upload        # grava firmware
pio run -t uploadfs      # grava data/ (KMLs + CSVs ground truth) no LittleFS
pio device monitor       # abre o terminal serial em 115200 baud
```

Saída esperada:

```text
=== POC VRA_Controlador (env: sim) ===
KMLs encontrados em /:
  1. ensaio_abcd.kml             [CSV ground truth: SIM]
  2. Sitio_Palmar.kml            [CSV ground truth: nao]
  3. talhao_completo.kml         [CSV ground truth: nao]
Escolha [1-3]: 1

Carregado: /ensaio_abcd.kml (4 zonas)
Carregado: /trajetoria_ensaio_abcd.csv (..., com dose Python)
Simulando: ........................................ concluido

=== RELATORIO FINAL ===
Zona     Alvo (kg/ha)    Aplicado medio    Erro %    Area coberta (m2)
1            60.0              ...           ...           ...
2            75.0              ...           ...           ...
3            90.0              ...           ...           ...
4           100.0              ...           ...           ...

Globais:
  Erro % global ponderado: ...
  Latencia logica.dose():  ~... us / chamada
  Latencia PID:            ~... us / iteracao

Paridade Python <-> ESP32 (... pontos):
  max |delta_dose|: ... kg/ha
  divergencias > 1e-3 kg/ha: 0
  veredicto: PASS

=== FIM ===
```

Salvar a saída em `_esp32/docs/sample_output_sim.txt`.

### Adicionar KMLs novos

Copie o `.kml` para `_esp32/data/`, rode `pio run -t uploadfs` e o menu
Serial vai listá-lo automaticamente no próximo boot. Para gerar o CSV
ground truth correspondente (validação cruzada Python ↔ ESP32):

```bash
cd _python
python scripts/export_trajectory_csv.py \
  --kml data/<seu>.kml \
  --out-csv ../_esp32/data/trajetoria_<seu>.csv \
  --width-m 20 --step-m 1
```

### Trocar idioma (PT → EN)

Edite `platformio.ini`, troque `-DLANG_PT` por `-DLANG_EN`, recompile.
Lógica e KMLs intactos.

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
