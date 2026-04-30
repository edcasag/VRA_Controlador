// Modo BUILD_ANALISE — sem hardware extra. Carrega KML do LittleFS, imprime
// sumário, CSV de auditoria e doses para 9 pontos demo cobrindo os 7 cenários
// do Algoritmo 1 (artigo SBIAGRO 2025). Selecionado via build_flags em
// platformio.ini: -DBUILD_ANALISE.
#ifdef BUILD_ANALISE
#pragma message "Compilando modo: BUILD_ANALISE"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include "Kml.h"
#include "LogicaHierarquica.h"

namespace {

constexpr const char* CAMINHO_KML = "/Sitio_Palmar.kml";

vra::Ponto centroidePoligono(const std::vector<vra::Ponto>& v) {
    vra::Ponto c {0.0, 0.0};
    if (v.empty()) return c;
    for (const auto& p : v) { c.x += p.x; c.y += p.y; }
    c.x /= v.size();
    c.y /= v.size();
    return c;
}

void imprimirCabecalhoPontos(Print& out) {
    out.println();
    out.println(F("=== 9 PONTOS DEMO ==="));
    out.println(F("Cenarios cobertos (Algoritmo 1, artigo SBIAGRO 2025):"));
    out.println(F("  1. Regiao circular (qualquer dose)"));
    out.println(F("  2. Poligono de exclusao (dose==0)"));
    out.println(F("  3. Poligono de inclusao (dose>0)"));
    out.println(F("  4. Sobreposicao -> menor area vence"));
    out.println(F("  5. IDW por pontos de referencia"));
    out.println(F("  6. Talhao (dose-base)"));
    out.println(F("  7. Fora do talhao -> 0"));
    out.println();
    out.println(F("idx  cenario                   x (m)      y (m)    dose (kg/ha)"));
    out.println(F("---  ------------------------  ---------  ---------  -----------"));
}

void imprimirLinhaPonto(Print& out, int idx, const char* descr,
                        double x, double y, double dose) {
    char buf[120];
    std::snprintf(buf, sizeof(buf),
                  "%-3d  %-24s  %9.2f  %9.2f  %11.2f",
                  idx, descr, x, y, dose);
    out.println(buf);
}

void rodarPontosDemo(const vra::Kml& kml, const vra::LogicaHierarquica& logica) {
    imprimirCabecalhoPontos(Serial);
    int idx = 1;

    if (kml.talhaoCarregado()) {
        const auto c = centroidePoligono(kml.talhao().vertices);
        imprimirLinhaPonto(Serial, idx++, "centroide do talhao",
                           c.x, c.y, logica.dose(c.x, c.y));
    }

    for (const auto& r : kml.regioesPoligonais()) {
        if (idx > 9) break;
        const auto c = centroidePoligono(r.vertices);
        char descr[40];
        std::snprintf(descr, sizeof(descr), "centroide poly '%s'", r.label.c_str());
        imprimirLinhaPonto(Serial, idx++, descr, c.x, c.y, logica.dose(c.x, c.y));
    }

    for (const auto& c : kml.regioesCirculares()) {
        if (idx > 9) break;
        char descr[40];
        std::snprintf(descr, sizeof(descr), "centro circ '%s'", c.label.c_str());
        imprimirLinhaPonto(Serial, idx++, descr, c.centro.x, c.centro.y,
                           logica.dose(c.centro.x, c.centro.y));
    }

    if (kml.talhaoCarregado() && idx <= 9) {
        const auto c = centroidePoligono(kml.talhao().vertices);
        const double offset = 50.0;
        const double x_fora = c.x + 1e6;
        imprimirLinhaPonto(Serial, idx++, "fora do talhao",
                           x_fora, c.y, logica.dose(x_fora, c.y));
        if (idx <= 9) {
            imprimirLinhaPonto(Serial, idx++, "talhao + offset",
                               c.x + offset, c.y + offset,
                               logica.dose(c.x + offset, c.y + offset));
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println();
    Serial.println(F("=== POC VRA_Controlador (env: analise) ==="));

    if (!LittleFS.begin(true)) {
        Serial.println(F("[ERRO] LittleFS nao montou"));
        return;
    }

    vra::Kml kml;
    if (!kml.carregarDoArquivo(CAMINHO_KML)) {
        Serial.printf("[ERRO] falha ao carregar %s do LittleFS\n", CAMINHO_KML);
        Serial.println(F("       (rode 'pio run -t uploadfs' primeiro)"));
        return;
    }

    kml.imprimirSumario(Serial);
    Serial.println();
    kml.imprimirCsv(Serial);

    vra::LogicaHierarquica logica(kml);
    rodarPontosDemo(kml, logica);

    Serial.println();
    Serial.println(F("=== FIM ==="));
}

void loop() {
    delay(60000);
}

#endif  // BUILD_ANALISE
