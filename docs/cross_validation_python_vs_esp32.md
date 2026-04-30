# Validação cruzada — Python (`VRA_Simulador`) ↔ ESP32 (`VRA_Controlador`)

**Objetivo.** Demonstrar que a Lógica Hierárquica do controlador
ESP32 (`vra::LogicaHierarquica::dose`) produz o mesmo resultado da
implementação Python (`vra_engine.dose_at`), confirmando paridade
algorítmica 1:1.

**KML de referência.** `Sitio_Palmar.kml` (10,85 ha; talhão `Field`
com `dose_base=0`, 6 polígonos de inclusão, 1 polígono de exclusão
`Sede`, 2 círculos de exclusão, 7 pontos de referência IDW). Origem
da projeção equiretangular local idêntica nos dois lados:
`lat=-23.3521368, lon=-46.9454401` (primeiro vértice do Talhão).

**Procedimento.** Mesmo método de projeção (equiretangular local
com origem no primeiro vértice do `Field`), mesmos parâmetros IDW
(potência 2, raio 100 m, `d_min=0,5 m`), mesma hierarquia (círculo →
polígono exclusão → polígono inclusão menor área → IDW → talhão
dose-base → 0). Avaliados 5 pontos cobrindo cenários distintos do
Algoritmo 1 (artigo SBIAGRO 2025).

## Resultado

| # | Cenário                  | x (m)    | y (m)    | dose Python | dose ESP32 | Δ |
|---|--------------------------|---------:|---------:|------------:|-----------:|--:|
| 1 | centróide do talhão      | -132,12  | -397,67  |   75,0000   |   75,00    | 0 |
| 2 | centróide poly *Poor*    |   42,52  | -417,85  |  100,0000   |  100,00    | 0 |
| 3 | centróide poly *Nobre*   | -217,70  | -482,40  |   80,0000   |   80,00    | 0 |
| 4 | centróide poly *Sede*    | -149,59  | -258,75  |    0,0000   |    0,00    | 0 |
| 5 | centro círculo excl. (1) | -154,15  | -360,60  |    0,0000   |    0,00    | 0 |

Paridade observada: **5/5**.

**Captura ESP32:** [`sample_output_analise.txt`](sample_output_analise.txt)
(modo `BUILD_ANALISE`, 9 pontos demo — destes, os 5 acima).

**Reprodução Python:**

```bash
cd _python
python -c "
import sys; sys.path.insert(0, '.')
from src.kml_parser import parse_kml
from src.vra_engine import dose_at
kml = parse_kml('data/Sitio Palmar.kml')
for nome, x, y in [
    ('centroide do talhao',   -132.12, -397.67),
    ('centroide poly Poor',     42.52, -417.85),
    ('centroide poly Nobre',  -217.70, -482.40),
    ('centroide poly Sede',   -149.59, -258.75),
    ('centro circ excl. (1)', -154.15, -360.60),
]:
    print(f'{nome:<26} {dose_at(x, y, kml):.4f}')
"
```

## Notas sobre precisão

A captura `BUILD_ANALISE` imprime dose com 2 casas decimais, suficiente
para validar paridade visual nestes 5 cenários — todos retornam **valor
exato por construção** (sem aritmética fracionária):

- **Cenários 3, 4, 5** (polígono inclusão, polígono exclusão, círculo):
  o algoritmo retorna `r.dose` ou `0.0` literal, sem cálculo. Paridade
  binária por design.
- **Cenário 1** (centróide do talhão): cai no ramo IDW, mas com **um
  único sample** dentro do raio de 100 m (o sample em `(-136,82, -415,29)`,
  distância ~18,2 m). IDW com 1 ponto degenera para `valor = sample.dose`,
  retornando `75,0` exato.
- **Cenário 2** (centróide poly *Poor*): cai em polígono de inclusão
  *Good* (`dose=100`), de menor área que o talhão circundante. Retorno
  literal.

Validação numérica de IDW **interpolado** (com múltiplos samples
contribuindo, gerando dose fracionária) não foi exercida por estes 5
pontos. Essa validação será coberta pelo modo `BUILD_SIM` da próxima
etapa, que percorre o talhão com trajetória boustrofédon densa,
exercitando milhares de avaliações IDW interpoladas e agregando o
erro contra o resultado Python.

## Conclusão

A paridade algorítmica entre `VRA_Simulador` (Python) e `VRA_Controlador`
(ESP32) está confirmada nos 5 cenários representativos do Algoritmo 1.
Origem de projeção, parâmetros IDW e ordem de hierarquia são idênticos
nas duas implementações, garantindo equivalência funcional para
auditoria por terceiros.

## Validação numérica em massa — modo `BUILD_SIM` (etapa 8)

A etapa 8 do POC adiciona o modo `BUILD_SIM`, que executa a Lógica
Hierárquica do ESP32 sobre uma **trajetória completa** gerada por
[`_python/scripts/export_trajectory_csv.py`](../../_python/scripts/export_trajectory_csv.py)
e versionada como [`_esp32/data/trajetoria_ensaio_abcd.csv`](../data/trajetoria_ensaio_abcd.csv)
(2856 fixes, ~190 KB, ensaio_abcd 200×200 m, largura 20 m, step 1 m).

Para cada fixe, o ESP32 calcula `dose_esp = LogicaHierarquica::dose(x, y)`
e compara contra a coluna `dose_alvo_kg_ha` do CSV (calculada pelo
Python `dose_at()`). A seção "Paridade Python ↔ ESP32" do relatório
final imprime:

- `max |delta_dose|` (kg/ha)
- número de divergências > 1e-3 kg/ha
- veredicto PASS/FAIL

Esse modo cobre o cenário que os 5 pontos não cobriam: **IDW interpolado
entre múltiplos samples** (que produz dose fracionária real). O CSV
versionado serve como teste unitário automático: qualquer regressão na
Lógica Hierárquica, no IDW, ou no port C++ do `Terreno` aparece
imediatamente como FAIL na captura do BUILD_SIM em
[`sample_output_sim.txt`](sample_output_sim.txt).
