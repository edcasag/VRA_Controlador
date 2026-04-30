#include "Relatorio.h"

#include <cmath>
#include <cstdio>

#include "i18n.h"

namespace vra {

namespace {

// Chave canonica de zona = arredondado para 1 casa decimal.
inline double chaveZona(double alvo) { return std::round(alvo * 10.0) / 10.0; }

}  // namespace

Relatorio::ZonaAcc& Relatorio::obterZona(double alvo) {
    const double k = chaveZona(alvo);
    for (auto& z : zonas_) {
        if (std::fabs(z.alvo_kg_ha - k) < 1e-6) return z;
    }
    zonas_.push_back({k, 0.0, 0, 0.0});
    return zonas_.back();
}

void Relatorio::acumular(double dose_alvo, double dose_aplicada,
                         double /*vel*/, double largura_m, double step_m) {
    auto& z = obterZona(dose_alvo);
    z.soma_aplicada += dose_aplicada;
    z.n_amostras += 1;
    z.area_m2 += largura_m * step_m;
}

void Relatorio::compararPython(double dose_esp, double dose_python) {
    const double diff = std::fabs(dose_esp - dose_python);
    if (diff > max_diff_) max_diff_ = diff;
    if (diff > TOL_PARIDADE) ++n_divergencias_;
    ++n_comparacoes_;
}

void Relatorio::registrarLatencias(double dose_us, double pid_us) {
    lat_dose_us_ = dose_us;
    lat_pid_us_  = pid_us;
}

void Relatorio::imprimir(Print& out) const {
    out.println();
    out.println(MSG_REPORT_TITLE);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%-6s %14s %16s %10s %18s",
                  MSG_COL_ZONE, MSG_COL_TARGET, MSG_COL_APPLIED,
                  MSG_COL_ERROR_PCT, MSG_COL_AREA);
    out.println(buf);

    double soma_ponderada_erro = 0.0;
    double soma_areas = 0.0;
    int idx = 1;
    for (const auto& z : zonas_) {
        const double aplicado_medio = (z.n_amostras > 0)
            ? z.soma_aplicada / static_cast<double>(z.n_amostras) : 0.0;
        const double erro_pct = (z.alvo_kg_ha > 0.0)
            ? (aplicado_medio - z.alvo_kg_ha) / z.alvo_kg_ha * 100.0 : 0.0;
        // Zona 0 (curvas / fora de aplicacao) eh ignorada na ponderacao global.
        if (z.alvo_kg_ha > 0.0) {
            soma_ponderada_erro += std::fabs(erro_pct) * z.area_m2;
            soma_areas += z.area_m2;
        }
        char nome[8];
        std::snprintf(nome, sizeof(nome), "%d", idx++);
        std::snprintf(buf, sizeof(buf),
                      "%-6s %14.1f %16.2f %+9.2f%% %17.0f",
                      nome, z.alvo_kg_ha, aplicado_medio, erro_pct, z.area_m2);
        out.println(buf);
    }

    out.println();
    out.println(MSG_GLOBALS);
    const double erro_global = (soma_areas > 0.0) ? soma_ponderada_erro / soma_areas : 0.0;
    std::snprintf(buf, sizeof(buf), MSG_GLOBAL_ERROR, erro_global);
    out.println(buf);
    std::snprintf(buf, sizeof(buf), MSG_LATENCY_DOSE, lat_dose_us_);
    out.println(buf);
    std::snprintf(buf, sizeof(buf), MSG_LATENCY_PID, lat_pid_us_);
    out.println(buf);

    if (n_comparacoes_ > 0) {
        out.println();
        std::snprintf(buf, sizeof(buf), MSG_PARITY_TITLE, static_cast<int>(n_comparacoes_));
        out.println(buf);
        std::snprintf(buf, sizeof(buf), MSG_PARITY_MAXDIFF, max_diff_);
        out.println(buf);
        std::snprintf(buf, sizeof(buf), MSG_PARITY_DIVERGE, static_cast<int>(n_divergencias_));
        out.println(buf);
        const char* veredicto = (n_divergencias_ == 0) ? MSG_VERDICT_PASS : MSG_VERDICT_FAIL;
        std::snprintf(buf, sizeof(buf), MSG_PARITY_VERDICT, veredicto);
        out.println(buf);
    }

    out.println();
    out.println(MSG_END);
}

}  // namespace vra
