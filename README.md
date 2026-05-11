# VRA_Controlador

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.19951135.svg)](https://doi.org/10.5281/zenodo.19951135)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)

Implementação em ESP32 do controlador de Aplicação em Taxa Variável (VRA) descrito no artigo apresentado no SBIAGRO 2025 ([PDF](docs/SBIAGRO2025_artigo.pdf), [slides](docs/SBIAGRO2025_apresentacao.pdf)). Lê zonas de manejo em arquivo KML do Google Earth, executa a Lógica Hierárquica de seleção de dose, mede a velocidade do trator por um sensor magnético na roda dianteira e aciona um atuador linear via controle PID com compensação contínua de velocidade.

## Arquitetura de tasks (BUILD_POC)

Três tarefas FreeRTOS, três frequências, papéis disjuntos:

| Task | Frequência | Entrada | Saída |
| --- | --- | --- | --- |
| `task_logica` (core 0) | 2 Hz | GPS NMEA (lat, lon, vel de fallback) | `rate_alvo` pela HDL |
| `task_velocidade` (core 0) | 10 Hz | Sensor de roda (GPIO 19, pulsos) | Recomputa `posicao_alvo_pct` com a velocidade fresca + `rate_alvo` em cache |
| `task_controle` (core 1) | 50 Hz | ADC do potenciômetro do atuador + tick do sensor de roda | PWM via PID |

Fusão de velocidade: o sensor de roda é a fonte primária. Trator parado (sem pulsos por > 1 s) faz `vel_atual = 0`, e a abertura do atuador FECHA por construção (segurança).

Trabalho de pesquisa de Edson Casagrande no programa de pós-graduação em Engenharia de Computação da Escola Politécnica da USP (POLI/USP), sob orientação do Prof. Carlos Eduardo Cugnasca.

![Visão geral do sistema](images/distribuidor_duplo_atuadores.jpg)

## Arquitetura

![Arquitetura do sistema](images/arquitetura_sistema.png)

## Pré-requisitos

- ESP32 DevKit V1 (board `esp32doit-devkit-v1`)
- [PlatformIO](https://platformio.org/) (extensão VSCode ou CLI)
- Cabo USB

## Instalação

```bash
git clone https://github.com/edcasag/VRA_Controlador.git
cd VRA_Controlador
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor
```

## Modos de build

Selecionados em `platformio.ini` via `-DBUILD_X`:

| Modo | Hardware adicional | Descrição |
|---|---|---|
| `BUILD_SIM` (default) | nenhum | Simulação completa standalone com GPS sintético e atuador simulado |
| `BUILD_ANALISE` | nenhum | Análise do KML carregado e cenários do Algoritmo 1 |
| `BUILD_TESTS` | nenhum | 9 testes automatizados |
| `BUILD_POC` | GPS, atuador, botões | POC completo com FreeRTOS |

Procedimento detalhado em [docs/README.md](docs/README.md).

## KMLs incluídos

| Arquivo | Conteúdo |
|---|---|
| `data/ensaio_abcd.kml` | 4 zonas A/B/C/D (1 ha cada, 90/75/60/100 kg/ha) |
| `data/talhao_completo.kml` | 7 zonas (50–100 kg/ha) |
| `data/Sitio_Palmar.kml` | Talhão real (14 vértices, 6 inclusões, 1 exclusão, 7 amostras IDW) |

Convenção dos nomes (campo `<name>` da Placemark KML):

| Sintaxe | Significado |
|---|---|
| `Field=0` ou nome qualquer | Polígono do talhão |
| `Label=Rate` (ex.: `Good=100`) | Polígono de inclusão |
| `Label=0` ou apenas `0` | Polígono de exclusão |
| `Label=Rate:Radius` (ex.: `Pedra=0:5m`) | Círculo de exclusão |
| Número (ex.: `120`) | Ponto de amostra IDW |

Quando duas zonas de inclusão se sobrepõem, vence a de menor área.

## Hardware (modo BUILD_POC)

PCB do controlador (placa EC-1.0):

![PCB do Controlador VRA](images/controlador_vra_pcb.jpg)

Esquemático completo: [images/esquema_prototipo.jpg](images/esquema_prototipo.jpg).

Sinais externos do ESP32 (placa EC-1.0 REV02):

| Sinal | GPIO | Função |
| --- | --- | --- |
| `VEL_TRATOR` | 19 | Sensor magnético na roda dianteira (pulsos pelos parafusos) |
| `FLUX` | 21 | Sensor de vazão do insumo (reservado, ainda não usado) |
| `GPS_RX` / `GPS_TX` | 16 / 17 | UART2 do receptor U-blox M8N |
| `PWM_M1_P` / `PWM_M1_N` | 25 / 26 | Acionamento do atuador linear (avançar / recuar) |
| `FEEDBACK_ADC` | 34 | Realimentação potenciométrica do atuador |
| `BTN_START` / `BTN_STOP` | 32 / 33 | Botões físicos de operação |

O sensor de roda passa pelo bloco SINAIS do esquema (condicionamento com resistor e diodo Zener), entregando ao ESP32 um sinal digital limpo.

A constante `D_PNEU_MM` em `src/SensorRoda.h` deve ser ajustada ao pneu específico do trator (ensaio de referência: pneu aro 24, ~1000 mm de diâmetro externo, 8 parafusos por revolução).

Montagem física para calibração (`m c1`/`m c2`/`m c3`):

![Montagem para calibração](images/calibracao.jpg)

## Validação

Validação cruzada entre o controlador e o simulador Python documentada em [docs/cross_validation_python_vs_esp32.md](docs/cross_validation_python_vs_esp32.md). Capturas de saída de cada modo em [docs/](docs/).

## Como citar

Software (Concept DOI, aponta para a última versão):

```text
Casagrande, E. (2026). VRA_Controlador. Zenodo.
https://doi.org/10.5281/zenodo.19951135
```

```bibtex
@software{casagrande2026vracontrolador,
  author    = {Casagrande, Edson},
  title     = {{VRA\_Controlador}},
  year      = 2026,
  publisher = {Zenodo},
  doi       = {10.5281/zenodo.19951135},
  url       = {https://doi.org/10.5281/zenodo.19951135}
}
```

Artigo correspondente (SBIAGRO 2025):

```bibtex
@inproceedings{casagrande2025sbiagro,
  author    = {Casagrande, Edson and Cugnasca, Carlos Eduardo},
  title     = {Controlador de Taxa Vari{\'a}vel para Distribuidores Agr{\'i}colas
               baseado em Google Earth e Interpola{\c{c}}{\~a}o IDW},
  booktitle = {Anais do XV Congresso Brasileiro de Agroinform{\'a}tica (SBIAGRO 2025)},
  year      = {2025}
}
```

## Licença

[MIT](LICENSE).

Bibliotecas vendored: [tinyxml2](https://github.com/leethomason/tinyxml2) (licença zlib).

## Software relacionado

[VRA_Simulador](https://github.com/edcasag/VRA_Simulador) — simulador em Python (DOI [10.5281/zenodo.19952166](https://doi.org/10.5281/zenodo.19952166)).

---

## English

ESP32 implementation of the Variable-Rate Application (VRA) controller described in the SBIAGRO 2025 paper. Reads management zones from a Google Earth KML, executes the Hierarchical Logic for dose selection, and drives a linear actuator via PID control.

Research work by Edson Casagrande at the Polytechnic School of the University of São Paulo (POLI/USP), under Prof. Carlos Eduardo Cugnasca.

### Quick install

```bash
git clone https://github.com/edcasag/VRA_Controlador.git
cd VRA_Controlador
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor
```

### License

[MIT](LICENSE).
