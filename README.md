# VRA_Controlador

[![DOI](https://zenodo.org/badge/1225653555.svg)](https://doi.org/10.5281/zenodo.19922431)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange.svg)](https://platformio.org/)

POC em ESP32 do controlador de Aplicação em Taxa Variável (VRA), complementar ao [VRA_Simulador](https://github.com/edcasag/VRA_Simulador) (Python). Lê zonas de manejo de um KML do Google Earth, executa a Lógica Hierárquica (polígonos de exclusão, polígonos de inclusão por menor área, IDW, talhão como dose-base), e aciona um atuador linear via PID com modelo de planta de 1ª ordem.

Implementa o controlador descrito no artigo apresentado no SBIAGRO 2025 ([PDF](docs/SBIAGRO2025_artigo.pdf), [slides](docs/SBIAGRO2025_apresentacao.pdf)). Trabalho de pesquisa de Edson Casagrande no programa de pós-graduação em Engenharia de Computação da Escola Politécnica da USP (POLI/USP), orientação Prof. Carlos Eduardo Cugnasca.

![Visão geral do sistema VRA_Controlador: KML no Google Earth → módulo ESP32 com GPS e WiFi → PWM aciona atuadores lineares X/Y dos discos espalhadores](images/distribuidor_duplo_atuadores.jpg)

> 🇬🇧 **English version**: see the section [English](#english) at the end of this document.

## Arquitetura do sistema

![Arquitetura do sistema VRA_Controlador](images/arquitetura_sistema.png)

O KML do Google Earth carrega zonas de manejo no PC/celular do operador. O módulo controle ESP32 recebe a posição do GNSS, executa a Lógica Hierárquica para determinar a dose-alvo no ponto atual, e aciona os atuadores via PWM. O laço fecha com feedback do potenciômetro do atuador (controle PID com planta de 1ª ordem).

## Complementaridade com o VRA_Simulador (Python)

| Aspecto | VRA_Simulador (Python) | VRA_Controlador (ESP32) |
|---|---|---|
| Foco | Simulação algorítmica idealizada | POC em hardware real (microcontrolador) |
| Trajetória | Boustrofédica completa com cabeceira | Boustrofédica simplificada no `BUILD_SIM` |
| Atuador | Aplicação instantânea da dose-alvo | Atuador linear com PID + planta de 1ª ordem |
| Tempo de resposta | — | TAU=0,3 s (servo) vs dt=0,2 s entre fixes |
| Saturação por vazão máxima | Não modela | Modela (constexpr na tabela de abertura) |
| Latência da Lógica Hierárquica | — | ~20–73 µs/chamada (medida no DevKit V1) |

A paridade numérica entre os dois (mesma origem de projeção, mesma fórmula IDW, mesmo `pointInPolygon`) foi validada bit-a-bit em 2010 fixes do ensaio A/B/C/D ([docs/cross_validation_python_vs_esp32.md](docs/cross_validation_python_vs_esp32.md)).

## Recursos

- Lê zonas de manejo de um KML do Google Earth (polígonos, círculos, pontos de amostra IDW), via [tinyxml2](https://github.com/leethomason/tinyxml2) vendored.
- Lógica Hierárquica fiel ao Algoritmo 1 do artigo SBIAGRO 2025: círculos → polígonos exclusão → polígonos inclusão (menor área vence) → IDW → talhão (dose-base) → 0.
- IDW Euclidiano `p=2`, raio 100 m, `d_min` 0,5 m.
- Atuador linear bidirecional via PWM (LEDC) com feedback potenciométrico (ADC).
- PID header-only (~30 linhas) com anti-windup por clamping.
- 4 modos plug-and-play via macro de build (`BUILD_SIM`, `BUILD_ANALISE`, `BUILD_TESTS`, `BUILD_POC`).
- Interface bilíngue (PT/EN) via `-DLANG_PT` / `-DLANG_EN` no `platformio.ini`.
- Latências medidas no DevKit V1: Lógica Hierárquica ~20–73 µs/chamada, PID ~5 µs/iteração — ~70× abaixo do critério ≤ 5 ms estabelecido no projeto.

## Pré-requisitos

- ESP32 DevKit V1 (ou compatível com `esp32doit-devkit-v1`).
- Cabo USB e driver USB-Serial (CP2102 ou CH340 conforme a placa).
- [PlatformIO](https://platformio.org/) (extensão do VSCode ou CLI).

Toolchain: PlatformIO + Arduino-ESP32 framework, C++17.

## Instalação

```bash
git clone https://github.com/edcasag/VRA_Controlador.git
cd VRA_Controlador
```

Pelo PlatformIO:

```bash
pio run                  # compila (default: BUILD_SIM + LANG_PT)
pio run -t upload        # grava firmware no ESP32
pio run -t uploadfs      # grava data/ (KMLs + CSVs) no LittleFS
pio device monitor       # abre terminal serial em 115200 baud
```

Pelo VSCode + PlatformIO IDE: `Ctrl+Alt+B` para build, ícone "✓" no canto inferior esquerdo para upload, ícone de plugue para abrir o monitor serial.

## Os 4 modos de build

O modo é selecionado em `platformio.ini` (descomente uma das três linhas de `build_flags`). Roda sempre `pio run` — o `#pragma message` no console identifica o modo compilado.

| Modo | Hardware exigido | O que faz |
|---|---|---|
| `BUILD_SIM` (default) | nada além do DevKit V1 | Simulação completa standalone: lê KML do LittleFS, gera trajetória boustrofédica, simula GPS sintético + atuador com PID + planta de 1ª ordem, agrega erro por zona, imprime relatório final estilo `coverage_report.py` do Python |
| `BUILD_ANALISE` | nada além do DevKit V1 | Sumário do KML + CSV de auditoria + 9 pontos demo cobrindo os 7 cenários do Algoritmo 1 |
| `BUILD_TESTS` | nada além do DevKit V1 | 9 testes cronometrados (parser, `pointInPolygon`, `polygonArea`, IDW, PID, tabela de abertura, latência ponta-a-ponta N=1000) |
| `BUILD_POC` | GPS UBlox M8N em UART2, atuador linear DC com PWM bidir + feedback ADC, botões START/STOP | POC com hardware completo, FreeRTOS (controle 50 Hz no core 1, lógica 2 Hz no core 0), 4 sub-modos `m e`/`m c1`/`m c2`/`m c3` (execução + 3 calibrações) |

Detalhes de procedimento, captura de saída e adição de novos KMLs em [docs/README.md](docs/README.md).

## KMLs incluídos

| Arquivo | Conteúdo | Uso |
|---|---|---|
| `data/ensaio_abcd.kml` | 4 zonas A/B/C/D, retangulares, 1 ha cada, doses 90/75/60/100 kg/ha | Ensaio integrado do artigo SBIAGRO 2025. CSV ground truth incluído (`trajetoria_ensaio_abcd.csv`) para validação cruzada Python ↔ ESP32 |
| `data/talhao_completo.kml` | 7 zonas retangulares na escala 50–100 kg/ha | Demonstração da legenda de cores |
| `data/Sitio_Palmar.kml` | Talhão real do autor: 14 vértices irregulares, 6 zonas de inclusão, 1 polígono de exclusão (Sede), 2 círculos (cupins/pedras), 7 amostras IDW | Validação em campo real |

Convenção dos nomes (campo `<name>` da Placemark KML):

| Feature | Sintaxe | Significado |
|---|---|---|
| Polígono talhão | `Field=0` ou nome qualquer (ex.: `Palmar`) | Contorno do talhão |
| Polígono inclusão (com label) | `Good=100` | Zona de aplicação com dose 100 kg/ha |
| Polígono inclusão (sem label) | `100` | Equivalente, com label autonumerado |
| Polígono exclusão | `Sede=0` ou apenas `0` | Não aplicar |
| Círculo (com label) | `Pedra=0:5m` | Exclusão circular de raio 5 m |
| Círculo (sem label) | `0:5m` | Idem, label autogerado |
| Ponto de amostra (IDW) | `120` | Amostra com taxa 120 kg/ha |

Espaços em torno do `=` e do `:` são tolerados. Quando duas zonas de inclusão se sobrepõem, **vence a de menor área**.

## Hardware

PCB do controlador (placa EC-1.0, projetada em torno do ESP32 DevKit V1):

![PCB do Controlador VRA, placa EC-1.0](images/controlador_vra_pcb.jpg)

Conectores: J1 (alimentação 5 V), J3 (MOSFETs/atuadores), J4 (displays I2C), J5 (encoder rotativo). Indicadores: LVCC (alimentação lógica), POWER (alimentação geral), L3V3/L5V (tensões), LCK (lock GPS), ANA (analógico).

Esquemático completo (CONTROLADOR EC-1.0 — ESP32):

![Esquemático completo do CONTROLADOR EC-1.0](images/esquema_prototipo.jpg)

Blocos: FONTES (alimentação), ESP32 (módulo principal), ROTARY DY-040 (encoder), GPS (UART), DISPLAYS / I2C, SINAIS, ANALÓGICAS XY/LASER (4–20 mA), MOSFETS (drivers de PWM), ADC ANALÓGICAS, SAÍDAS.

A calibração das três rotinas do `BUILD_POC` (cal1: extremos do potenciômetro; cal2: tabela posição→abertura; cal3: fator de escoamento) refere-se à seguinte montagem física:

![Procedimento de calibração](images/calibracao.jpg)

## Estrutura do repositório

```text
.
├── platformio.ini           # 1 env, 3 modos via macro -DBUILD_X
├── src/
│   ├── Geometria.h          # Ponto, projeção equiretangular, polygonArea, pointInPolygon
│   ├── Kml.{h,cpp}          # parsing tinyxml2: Talhao, RegiaoPoligonal, RegiaoCircular, PontoReferencia
│   ├── LogicaHierarquica.{h,cpp}  # Algoritmo 1 + IDW p=2
│   ├── Pid.h                # PID header-only com anti-windup
│   ├── Atuador.h            # tabela de abertura, posicao_alvo_para_taxa, AlvoAtuador
│   ├── Terreno.h            # declive linear + bossas Gaussianas (espelha terrain.py)
│   ├── Planta.h             # planta de 1ª ordem TAU=0,3 s
│   ├── Trajetoria.{h,cpp}   # boustrofédon simplificado
│   ├── CsvReader.h          # streaming linha-a-linha do CSV ground truth
│   ├── Relatorio.{h,cpp}    # agregador por zona + paridade Python<->ESP32
│   ├── MenuSerial.{h,cpp}   # menu interativo de seleção do KML
│   ├── i18n.h               # ~30 macros PT/EN
│   ├── main_sim.cpp         # entry point BUILD_SIM (default)
│   ├── main_analise.cpp     # entry point BUILD_ANALISE
│   ├── main_tests.cpp       # entry point BUILD_TESTS
│   └── main_poc.cpp         # entry point BUILD_POC (FreeRTOS)
├── lib/tinyxml2/            # tinyxml2 vendored (zlib license)
├── data/                    # KMLs + CSVs ground truth (gravados no LittleFS via uploadfs)
├── images/                  # diagramas, fotos do PCB, esquemático, calibração
└── docs/
    ├── README.md                            # procedimento de upload + captura
    ├── cross_validation_python_vs_esp32.md  # validação cruzada (5 pontos + 2010 em massa)
    ├── sample_output_analise.txt            # captura BUILD_ANALISE
    ├── sample_output_tests.txt              # captura BUILD_TESTS (9/9 PASS)
    ├── sample_output_sim.txt                # captura BUILD_SIM (paridade 2010/2010 PASS)
    ├── SBIAGRO2025_artigo.pdf               # artigo aceito no SBIAGRO 2025
    └── SBIAGRO2025_apresentacao.pdf         # slides da apresentação no SBIAGRO 2025
```

## Validação cruzada Python ↔ ESP32

Dois níveis de validação numérica documentados em [docs/cross_validation_python_vs_esp32.md](docs/cross_validation_python_vs_esp32.md):

1. **5 pontos isolados** do Sítio Palmar cobrindo cenários distintos do Algoritmo 1 (talhão, inclusão Poor→Good, inclusão Nobre, exclusão Sede, exclusão circular). Origem da projeção idêntica nos dois lados.
2. **2010 pontos em massa** do ensaio A/B/C/D, comparando `dose_at` (Python) vs `dose` (ESP32) ao longo de uma trajetória boustrofédica completa. Resultado: `max |delta_dose| = 0.00 kg/ha`, **paridade bit-a-bit**.

Procedimento para gerar o CSV ground truth de um KML novo:

```bash
cd _python
python scripts/export_trajectory_csv.py \
  --kml data/<seu>.kml \
  --out-csv ../_esp32/data/trajetoria_<seu>.csv \
  --width-m 20 --step-m 1
```

## Achados científicos

A paridade da Lógica Hierárquica é PASS, mas o erro de aplicação simulado no `BUILD_SIM` fica em -15% a -41% por zona. Esse erro **não** vem do algoritmo — vem da inércia mecânica do servo (TAU=0,3 s vs dt≈0,2 s entre fixes). Em transições de zona, o setpoint muda antes de a planta convergir.

Esse achado é o ponto da complementaridade ESP32 ↔ Python: o `VRA_Simulador` Python assume aplicação instantânea da dose-alvo (sem PID, sem inércia, sem histerese). O `VRA_Controlador` ESP32 cumpre o papel de quantificar a contribuição do atuador físico para o erro total.

## Como citar

Software (use o **Concept DOI** permanente do Zenodo, que sempre aponta pra última versão):

```text
Casagrande, E. (2026). VRA_Controlador. Zenodo.
https://doi.org/10.5281/zenodo.19922431
```

BibTeX:

```bibtex
@software{casagrande2026vracontrolador,
  author    = {Casagrande, Edson},
  title     = {{VRA\_Controlador: POC em ESP32 do Controlador de Aplica\c{c}\~{a}o em Taxa Vari\'{a}vel}},
  year      = 2026,
  publisher = {Zenodo},
  doi       = {10.5281/zenodo.19922431},
  url       = {https://doi.org/10.5281/zenodo.19922431}
}
```

Para citar uma versão **específica** (v1.0.0, DOI `10.5281/zenodo.19922432`), use o Version DOI listado em <https://zenodo.org/records/19922432>.

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

Ver `CITATION.cff` na raiz para metadados estruturados (lidos por GitHub e Zenodo).

## Licença

[MIT](LICENSE) — uso livre para fins acadêmicos e comerciais, com atribuição.

Bibliotecas vendored:

- [tinyxml2](https://github.com/leethomason/tinyxml2) (Lee Thomason) — licença zlib (cabeçalho dos próprios arquivos satisfaz atribuição).

## Autor

**Edson Casagrande**
Mestrando em Engenharia da Computação, POLI/USP
Orientador: Prof. Carlos Eduardo Cugnasca
GitHub: [@edcasag](https://github.com/edcasag)

---

## English

VRA_Controlador is an ESP32 proof-of-concept of a Variable-Rate Application (VRA) controller, complementary to [VRA_Simulador](https://github.com/edcasag/VRA_Simulador) (Python). It reads management zones from a Google Earth KML, runs the Hierarchical Logic (exclusion polygons, inclusion polygons by smallest area, IDW, field as base rate), and drives a linear actuator through a PID controller with a first-order plant model.

It implements the controller described in the SBIAGRO 2025 paper. Research work by Edson Casagrande at the Polytechnic School of the University of São Paulo (POLI/USP), under Prof. Carlos Eduardo Cugnasca.

### Complementarity with VRA_Simulador (Python)

The Python simulator models idealized algorithmic behavior (instant dose application). The ESP32 POC adds the physical-actuator side: PID, first-order plant, opening tables, saturation by maximum flow. Numerical parity (same projection origin, same IDW formula, same `pointInPolygon`) was validated bit-for-bit on 2010 fixes of the A/B/C/D experiment.

### Quick install

```bash
git clone https://github.com/edcasag/VRA_Controlador.git
cd VRA_Controlador
pio run                  # build (default: BUILD_SIM + LANG_PT)
pio run -t upload        # flash firmware
pio run -t uploadfs      # flash data/ (KMLs + CSVs) to LittleFS
pio device monitor       # open serial monitor at 115200 baud
```

To switch to English UI, edit `platformio.ini`, swap `-DLANG_PT` for `-DLANG_EN`, and rebuild.

### Build modes

Selected via `-DBUILD_X` in `platformio.ini`. Always run `pio run`.

- `BUILD_SIM` (default): standalone simulation on a bare DevKit V1, with synthetic GPS, simulated actuator, and a final coverage report.
- `BUILD_ANALISE`: KML summary + CSV audit + 9 demo points covering all 7 branches of Algorithm 1.
- `BUILD_TESTS`: 9 timed tests including end-to-end latency benchmark (≤ 5 ms criterion, measured ~73 µs).
- `BUILD_POC`: full hardware POC with GPS, actuator, PID, FreeRTOS tasks (50 Hz control on core 1, 2 Hz logic on core 0).

### Cross-validation

See [docs/cross_validation_python_vs_esp32.md](docs/cross_validation_python_vs_esp32.md). Bit-for-bit parity in 2010 in-mass points (`max |delta| = 0.00 kg/ha`).

### License

[MIT](LICENSE).
