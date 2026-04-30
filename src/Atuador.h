// Matemática do atuador linear (sem hardware): tabela posição -> abertura
// física, fator de escoamento, e cadeia "taxa -> vazão -> abertura -> posição".
//
// As constantes V_MIN, V_MAX (extremos do potenciômetro) ficam em main_poc.cpp,
// pois dependem de hardware. Aqui só o que é puramente algébrico — testável
// pelo env "tests" sem ESP32 físico.
#pragma once

#include <algorithm>

namespace vra {

// Tabela posição%(curso) -> abertura física (fração 0..1 do escoamento máximo).
// 11 pontos com interpolação linear. Substituir após cal2 de campo.
struct CalibPonto { double posicao_pct; double abertura; };

constexpr CalibPonto TABELA_ABERTURA[11] = {
    {  0.0, 0.00 },
    { 10.0, 0.05 },
    { 20.0, 0.12 },
    { 30.0, 0.22 },
    { 40.0, 0.34 },
    { 50.0, 0.48 },
    { 60.0, 0.62 },
    { 70.0, 0.75 },
    { 80.0, 0.86 },
    { 90.0, 0.94 },
    {100.0, 1.00 },
};

// Vazão de produto em kg/min quando abertura = 1.0 (cal3).
constexpr double FATOR_ESCOAMENTO_KG_MIN = 30.0;

// Largura da faixa de aplicação do distribuidor.
constexpr double LARGURA_M = 20.0;

// Abaixo deste limiar, o atuador recolhe (fecha) — evita aplicação durante
// paradas e ruído GNSS quando parado.
constexpr double V_MIN_OPERACAO_KMH = 1.0;

// Abertura em função da posição (interpolação linear na tabela).
inline double abertura_em(double posicao_pct) {
    if (posicao_pct <= TABELA_ABERTURA[0].posicao_pct)   return TABELA_ABERTURA[0].abertura;
    if (posicao_pct >= TABELA_ABERTURA[10].posicao_pct)  return TABELA_ABERTURA[10].abertura;
    int i = static_cast<int>(posicao_pct / 10.0);
    if (i > 9) i = 9;
    const double p0 = TABELA_ABERTURA[i].posicao_pct;
    const double p1 = TABELA_ABERTURA[i + 1].posicao_pct;
    const double a0 = TABELA_ABERTURA[i].abertura;
    const double a1 = TABELA_ABERTURA[i + 1].abertura;
    const double t  = (posicao_pct - p0) / (p1 - p0);
    return a0 + t * (a1 - a0);
}

// Inverso: posição em função da abertura desejada.
inline double posicao_para_abertura(double abertura_alvo) {
    abertura_alvo = std::clamp(abertura_alvo, 0.0, 1.0);
    for (int i = 0; i < 10; ++i) {
        const double a0 = TABELA_ABERTURA[i].abertura;
        const double a1 = TABELA_ABERTURA[i + 1].abertura;
        if ((abertura_alvo >= a0 && abertura_alvo <= a1) ||
            (abertura_alvo <= a0 && abertura_alvo >= a1)) {
            const double denom = a1 - a0;
            const double t     = (denom != 0.0) ? (abertura_alvo - a0) / denom : 0.0;
            const double p0    = TABELA_ABERTURA[i].posicao_pct;
            const double p1    = TABELA_ABERTURA[i + 1].posicao_pct;
            return p0 + t * (p1 - p0);
        }
    }
    return 100.0;
}

struct AlvoAtuador {
    double posicao_pct;       // setpoint para o PID (0..100)
    double vazao_kg_min;      // antes da saturação
    double abertura_alvo;     // após saturação em [0, 1]
    bool   saturou;           // true se vazão excedeu o fator de escoamento
};

// Cadeia "taxa -> vazão -> abertura -> posição" do Cap. 6 da tese.
// vazão_kg_min = taxa[kg/ha] · velocidade[km/h] · largura[m] / 600
inline AlvoAtuador posicao_alvo_para_taxa(double rate_kg_ha, double vel_kmh,
                                          double largura_m       = LARGURA_M,
                                          double fator_kg_min    = FATOR_ESCOAMENTO_KG_MIN,
                                          double v_min_op_kmh    = V_MIN_OPERACAO_KMH) {
    AlvoAtuador a {0.0, 0.0, 0.0, false};
    if (vel_kmh < v_min_op_kmh || rate_kg_ha <= 0.0) return a;
    a.vazao_kg_min  = rate_kg_ha * vel_kmh * largura_m / 600.0;
    a.abertura_alvo = a.vazao_kg_min / fator_kg_min;
    if (a.abertura_alvo > 1.0) {
        a.abertura_alvo = 1.0;
        a.saturou       = true;
    }
    a.posicao_pct = posicao_para_abertura(a.abertura_alvo);
    return a;
}

}  // namespace vra
