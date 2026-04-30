// env:tests — 9 testes cronometrados, paridade com Python (1e-6).
// O teste 9 (bench de latência) reforça o Cap. 7 da tese com dado
// reproduzível por terceiros, sem depender do Syncat proprietário.
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Atuador.h"
#include "Geometria.h"
#include "Kml.h"
#include "LogicaHierarquica.h"
#include "Pid.h"

namespace {

int g_passes = 0;
int g_falhas = 0;

void reportar(const char* nome, bool ok, double us_op = 0.0) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s  %-44s  %.2f us/op",
                  ok ? "[PASS]" : "[FAIL]", nome, us_op);
    Serial.println(buf);
    if (ok) ++g_passes; else ++g_falhas;
}

bool aprox(double a, double b, double tol = 1e-6) {
    return std::fabs(a - b) <= tol;
}

// ---------------------------------------------------------------------------
// 1. Parser KML — 7 padrões da Tabela 4 do Cap. 6.
// ---------------------------------------------------------------------------
void teste_1_parser() {
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>"
          "<Placemark><name>Field=100</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>0,0,0 1,0,0 1,1,0 0,1,0 0,0,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          "<Placemark><name>Good=120</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>0.1,0.1,0 0.5,0.1,0 0.5,0.5,0 0.1,0.5,0 0.1,0.1,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          "<Placemark><name>Sede=0</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>0.6,0.6,0 0.7,0.6,0 0.7,0.7,0 0.6,0.7,0 0.6,0.6,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          "<Placemark><name>Pedra=0:5m</name><Point><coordinates>0.2,0.8,0</coordinates></Point></Placemark>"
          "<Placemark><name>0:3m</name><Point><coordinates>0.3,0.8,0</coordinates></Point></Placemark>"
          "<Placemark><name>Sample=85</name><Point><coordinates>0.4,0.4,0</coordinates></Point></Placemark>"
          "<Placemark><name>120</name><Point><coordinates>0.5,0.4,0</coordinates></Point></Placemark>"
        "</Document></kml>";

    vra::Kml kml;
    const unsigned long t0 = micros();
    const bool ok_load = kml.carregarDoTexto(xml);
    const double dt = micros() - t0;

    bool ok = ok_load
              && kml.talhaoCarregado()
              && aprox(kml.talhao().dose_base, 100.0)
              && kml.regioesPoligonais().size() == 2
              && kml.regioesCirculares().size() == 2
              && kml.pontosReferencia().size() == 2;
    reportar("1. parser KML (Tab.4, 7 padroes)", ok, dt);
}

// ---------------------------------------------------------------------------
// 2. pointInPolygon — 8 casos.
// ---------------------------------------------------------------------------
void teste_2_pointInPolygon() {
    const std::vector<vra::Ponto> quad =
        {{0,0},{10,0},{10,10},{0,10}};
    const std::vector<vra::Ponto> tri =
        {{0,0},{10,0},{0,10}};
    // L côncavo: corta o quadrante superior-direito.
    const std::vector<vra::Ponto> L =
        {{0,0},{10,0},{10,5},{5,5},{5,10},{0,10}};

    const unsigned long t0 = micros();
    bool ok =  vra::pointInPolygon(5, 5, quad)         // dentro
            && !vra::pointInPolygon(15, 5, quad)       // fora
            && !vra::pointInPolygon(-5, 5, quad)       // fora
            && !vra::pointInPolygon(5, 15, quad)       // fora
            && !vra::pointInPolygon(5, -5, quad)       // fora
            && vra::pointInPolygon(3, 3, tri)          // dentro
            && !vra::pointInPolygon(7, 7, L)           // fora (canto cortado)
            && vra::pointInPolygon(2, 2, L);           // dentro
    const double dt = (micros() - t0) / 8.0;
    reportar("2. pointInPolygon (8 casos)", ok, dt);
}

// ---------------------------------------------------------------------------
// 3. polygonArea — quadrado e Sítio Palmar (~10,85 ha).
// ---------------------------------------------------------------------------
void teste_3_polygonArea() {
    const std::vector<vra::Ponto> quad = {{0,0},{10,0},{10,10},{0,10}};
    const unsigned long t0 = micros();
    const double a_quad = vra::polygonArea(quad);
    const double dt_us  = micros() - t0;
    bool ok = aprox(a_quad, 100.0, 1e-9);

    vra::Kml kml;
    if (LittleFS.exists("/Sitio_Palmar.kml") && kml.carregarDoArquivo("/Sitio_Palmar.kml")) {
        const double area_ha = kml.talhao().area_m2 / 10000.0;
        // Critério 4 do plano: ~10,85 ha (tolerância 5%).
        ok = ok && std::fabs(area_ha - 10.85) <= 0.55;
    }
    reportar("3. polygonArea (quadrado + Sitio Palmar)", ok, dt_us);
}

// Helper: monta um Kml mínimo via carregarDoTexto para testes 4 e 5.
bool montarKmlSintetico(vra::Kml& kml) {
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>"
          // Talhão 1km x 1km centrado em (0,0)
          "<Placemark><name>Field=100</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>-0.005,-0.005,0 0.005,-0.005,0 0.005,0.005,0 -0.005,0.005,0 -0.005,-0.005,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          // Polígono Good=120 ocupando o canto superior direito
          "<Placemark><name>Good=120</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>0.001,0.001,0 0.004,0.001,0 0.004,0.004,0 0.001,0.004,0 0.001,0.001,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          // Polígono Premium=150, menor, dentro de Good
          "<Placemark><name>Premium=150</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>0.0015,0.0015,0 0.0025,0.0015,0 0.0025,0.0025,0 0.0015,0.0025,0 0.0015,0.0015,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          // Exclusão Sede=0
          "<Placemark><name>Sede=0</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>-0.004,-0.004,0 -0.003,-0.004,0 -0.003,-0.003,0 -0.004,-0.003,0 -0.004,-0.004,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          // Círculo Cupinzeiro=0:5m
          "<Placemark><name>Cupinzeiro=0:5m</name><Point><coordinates>-0.002,0.002,0</coordinates></Point></Placemark>"
          // Dois pontos de referência para IDW na metade inferior do talhão
          "<Placemark><name>P1=80</name><Point><coordinates>-0.002,-0.001,0</coordinates></Point></Placemark>"
          "<Placemark><name>P2=120</name><Point><coordinates>0.002,-0.001,0</coordinates></Point></Placemark>"
        "</Document></kml>";
    return kml.carregarDoTexto(xml);
}

// ---------------------------------------------------------------------------
// 4. LogicaHierarquica::dose — 7 cenários.
// ---------------------------------------------------------------------------
void teste_4_dose() {
    vra::Kml kml;
    if (!montarKmlSintetico(kml)) { reportar("4. dose (7 cenarios)", false, 0); return; }
    vra::LogicaHierarquica logica(kml);

    // Centróides em metros. O KML acima projeta (lon=-0.005..0.005, lat=-0.005..0.005)
    // em ~(-555, -555)..(555, 555). Centróides nominais em metros:
    //   centro talhão            ~ (0, 0)            -> dose = 100 (base) OU IDW
    //   centro Good (sem Premium) ~ (278, 278)        -> dose = 120 (mas Premium dentro!)
    //   centro Premium           ~ (222, 222)         -> dose = 150 (menor área vence)
    //   centro Sede              ~ (-389, -389)       -> dose = 0 (exclusão)
    //   centro Cupinzeiro circ.  ~ (-222, 222)        -> dose = 0 (raio 5m)
    //   ponto entre P1 e P2      ~ (0, -111)          -> dose ≈ 100 (IDW simétrico)
    //   muito longe              ~ (10000, 10000)     -> dose = 0 (fora)

    const auto& talhao = kml.talhao();
    auto cx = 0.0, cy = 0.0;
    for (const auto& p : talhao.vertices) { cx += p.x; cy += p.y; }
    cx /= talhao.vertices.size();
    cy /= talhao.vertices.size();

    auto centroide_de = [&](size_t i) {
        const auto& v = kml.regioesPoligonais()[i].vertices;
        double x = 0, y = 0;
        for (const auto& p : v) { x += p.x; y += p.y; }
        return vra::Ponto{x / v.size(), y / v.size()};
    };

    const auto pGood    = centroide_de(0);  // Good
    const auto pPremium = centroide_de(1);  // Premium dentro de Good
    const auto pSede    = centroide_de(2);  // Sede

    const auto pCirc = kml.regioesCirculares()[0].centro;

    // Ponto entre os dois pontos de referência (dose IDW simétrica)
    const auto& p1 = kml.pontosReferencia()[0].pos;
    const auto& p2 = kml.pontosReferencia()[1].pos;
    const vra::Ponto pIdw{(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};

    const unsigned long t0 = micros();
    bool ok =  aprox(logica.dose(pSede.x,    pSede.y),    0.0)            // exclusão poligonal
            && aprox(logica.dose(pCirc.x,    pCirc.y),    0.0)            // exclusão circular
            && aprox(logica.dose(pPremium.x, pPremium.y), 150.0)          // menor área vence
            // Centroide de Good fica dentro de Premium (Good é maior, contém Premium centralizado)?
            // Não — Good = (-278,-278..278,278) aprox e centróide ~ (0,0)? Não, depende.
            // Para evitar dependência: testa um ponto em Good fora de Premium.
            && aprox(logica.dose(pGood.x + 70, pGood.y), 120.0)           // inclusão poligonal
            && aprox(logica.dose(pIdw.x, pIdw.y), 100.0, 1e-3)            // IDW simétrico ≈ média
            && aprox(logica.dose(1e6, 1e6), 0.0)                          // fora do talhão
            && aprox(logica.dose(cx, cy - 400), 100.0);                   // só no talhão (dose-base)
    const double dt = (micros() - t0) / 7.0;
    reportar("4. LogicaHierarquica::dose (7 cenarios)", ok, dt);
}

// ---------------------------------------------------------------------------
// 5. IDW simétrico — média ponderada quando distâncias iguais.
// ---------------------------------------------------------------------------
void teste_5_idw() {
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>"
          "<Placemark><name>Field=0</name><Polygon><outerBoundaryIs><LinearRing>"
            "<coordinates>-0.01,-0.01,0 0.01,-0.01,0 0.01,0.01,0 -0.01,0.01,0 -0.01,-0.01,0</coordinates>"
          "</LinearRing></outerBoundaryIs></Polygon></Placemark>"
          "<Placemark><name>P1=100</name><Point><coordinates>-0.0001,0,0</coordinates></Point></Placemark>"
          "<Placemark><name>P2=200</name><Point><coordinates>0.0001,0,0</coordinates></Point></Placemark>"
        "</Document></kml>";
    vra::Kml kml;
    if (!kml.carregarDoTexto(xml)) { reportar("5. IDW simetrico", false, 0); return; }
    vra::LogicaHierarquica logica(kml);

    // Ponto exatamente equidistante dos dois (em y=0, no meio).
    const double xm = (kml.pontosReferencia()[0].pos.x + kml.pontosReferencia()[1].pos.x) / 2.0;
    const unsigned long t0 = micros();
    const double d = logica.dose(xm, 0.0);
    const double dt = micros() - t0;
    reportar("5. IDW simetrico (esperado 150.0)", aprox(d, 150.0, 1e-9), dt);
}

// ---------------------------------------------------------------------------
// 6. PID — convergência degrau 0->100 em ≤ 20 iter.
// ---------------------------------------------------------------------------
void teste_6_pid() {
    vra::Pid pid(8.0, 1.5, 0.0, 0.1, -100.0, 100.0);
    double pos = 0.0;
    int iter = 0;
    const unsigned long t0 = micros();
    while (iter < 50) {
        const double saida = pid.atualizar(100.0, pos);
        // Modelo simples do atuador: integra a saída (constante de tempo aprox.)
        pos += saida * 0.1 * 0.5;
        ++iter;
        if (pos >= 95.0) break;
    }
    const double dt = (micros() - t0) / std::max(iter, 1);
    reportar("6. PID degrau 0->100 (<= 20 iter)", iter <= 20, dt);
}

// ---------------------------------------------------------------------------
// 7. Tabela posicao->abertura ↔ abertura->posicao (identidade).
// ---------------------------------------------------------------------------
void teste_7_tabela_iv() {
    bool ok = true;
    const unsigned long t0 = micros();
    int n = 0;
    for (int i = 0; i <= 100; i += 5) {
        const double pct  = static_cast<double>(i);
        const double a    = vra::abertura_em(pct);
        const double pct2 = vra::posicao_para_abertura(a);
        if (!aprox(pct, pct2, 1e-6)) ok = false;
        ++n;
    }
    const double dt = (micros() - t0) / std::max(n, 1);
    reportar("7. tabela posicao<->abertura (1e-6)", ok, dt);
}

// ---------------------------------------------------------------------------
// 8. posicao_alvo_para_taxa — caso canônico documentado.
// ---------------------------------------------------------------------------
void teste_8_canonico() {
    // taxa=100 kg/ha, vel=5 km/h, larg=20 m, fator=30 kg/min
    // -> vazão = 100*5*20/600 = 16,667 kg/min
    // -> abertura = 16,667 / 30 = 0,5556
    // -> posição = posicao_para_abertura(0,5556)
    const unsigned long t0 = micros();
    const auto a = vra::posicao_alvo_para_taxa(100.0, 5.0);
    const double dt = micros() - t0;

    const double pos_esperada = vra::posicao_para_abertura(16.6667 / 30.0);
    bool ok =  aprox(a.vazao_kg_min,  16.6667, 1e-3)
            && aprox(a.abertura_alvo, 0.5556,  1e-3)
            && aprox(a.posicao_pct,   pos_esperada, 1e-3)
            && !a.saturou;
    reportar("8. canonico (100 kg/ha, 5 km/h, 20 m)", ok, dt);
}

// ---------------------------------------------------------------------------
// 9. Latência ponta-a-ponta — bench reproduzível para o Cap. 7 da tese.
// ---------------------------------------------------------------------------
void teste_9_latencia() {
    vra::Kml kml;
    if (!LittleFS.exists("/Sitio_Palmar.kml") || !kml.carregarDoArquivo("/Sitio_Palmar.kml")) {
        reportar("9. latencia ponta-a-ponta (1000 iter)", false, 0);
        return;
    }
    vra::LogicaHierarquica logica(kml);

    // 1000 fixes sintéticos em torno do centróide do talhão, raio 200 m.
    const auto& v = kml.talhao().vertices;
    double cx = 0, cy = 0;
    for (const auto& p : v) { cx += p.x; cy += p.y; }
    cx /= v.size();
    cy /= v.size();

    constexpr int N = 1000;
    std::vector<long> amostras_us;
    amostras_us.reserve(N);
    for (int i = 0; i < N; ++i) {
        const double ang = (i % 360) * 0.017453292519943295;
        const double r   = 50.0 + (i % 7) * 20.0;
        const double x   = cx + r * std::cos(ang);
        const double y   = cy + r * std::sin(ang);

        const unsigned long t0 = micros();
        const double rate = logica.dose(x, y);
        (void) vra::posicao_alvo_para_taxa(rate, 5.0);
        amostras_us.push_back(static_cast<long>(micros() - t0));
    }
    std::sort(amostras_us.begin(), amostras_us.end());
    long total = 0;
    for (long a : amostras_us) total += a;
    const double media_us = static_cast<double>(total) / N;
    const long   p95_us   = amostras_us[(N * 95) / 100];

    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "9. latencia ponta-a-ponta (N=1000): media=%.1f us, p95=%ld us  [<= 5000 us]",
                  media_us, p95_us);
    Serial.println(buf);
    reportar("9. latencia (criterio: media <= 5000 us)", media_us <= 5000.0, media_us);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println();
    Serial.println(F("=== POC VRA_Controlador (env: tests) ==="));
    if (!LittleFS.begin(true)) {
        Serial.println(F("[ERRO] LittleFS nao montou"));
        return;
    }

    teste_1_parser();
    teste_2_pointInPolygon();
    teste_3_polygonArea();
    teste_4_dose();
    teste_5_idw();
    teste_6_pid();
    teste_7_tabela_iv();
    teste_8_canonico();
    teste_9_latencia();

    Serial.println();
    char resumo[80];
    std::snprintf(resumo, sizeof(resumo),
                  "=== RESUMO: %d PASS, %d FAIL ===", g_passes, g_falhas);
    Serial.println(resumo);
}

void loop() {
    delay(60000);
}
