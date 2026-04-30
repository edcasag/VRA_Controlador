// Agregador de erro por zona + paridade Python<->ESP32 + impressao final.
// Equivalente em escopo ao coverage_report.py do VRA_Simulador Python:
// soma massa aplicada e area coberta por chave de dose-alvo, calcula erro%.
// Adicionalmente, se o BUILD_SIM tem CSV ground truth, acumula tambem a
// diferenca dose_esp - dose_python e reporta veredicto PASS/FAIL.
#pragma once

#include <Arduino.h>

#include <vector>

namespace vra {

class Relatorio {
public:
    Relatorio() = default;

    // Acumula um fixe processado.
    // largura_m * step_m = area incremental coberta (assume passo = step_m).
    void acumular(double dose_alvo_kg_ha, double dose_aplicada_kg_ha,
                  double vel_mps, double largura_m, double step_m);

    // Compara dose calculada pelo ESP32 com a esperada pelo Python (do CSV).
    void compararPython(double dose_esp, double dose_python);

    // Marca medicoes globais (latencias).
    void registrarLatencias(double dose_us_media, double pid_us_media);

    void imprimir(Print& out) const;

private:
    struct ZonaAcc {
        double alvo_kg_ha;
        double soma_aplicada;       // soma de dose_aplicada
        long   n_amostras;
        double area_m2;
    };
    std::vector<ZonaAcc> zonas_;

    // Encontra/cria zona com chave = round(alvo, 1).
    ZonaAcc& obterZona(double alvo);

    // Paridade Python<->ESP32.
    double max_diff_ = 0.0;
    long   n_divergencias_ = 0;
    long   n_comparacoes_ = 0;
    static constexpr double TOL_PARIDADE = 1e-3;

    // Latencias (us).
    double lat_dose_us_ = 0.0;
    double lat_pid_us_  = 0.0;
};

}  // namespace vra
