// LogicaHierarquica — Algoritmo 1 do Cap. 6 da tese.
// Hierarquia de decisão (cap. 6 §6.3):
//   1. Região circular (qualquer dose, exclusão tem mesma forma)
//   2. Polígono de exclusão (dose==0)
//   3. Polígono de inclusão (dose>0): se sobrepostos, vence o de menor área
//   4. IDW p=2 nos pontos de referência (raio 100 m, d_min 0,5 m)
//   5. Talhão (dose-base) se ponto dentro
//   6. Fora do talhão -> 0
#pragma once

#include "Kml.h"

namespace vra {

constexpr double IDW_POTENCIA = 2.0;
constexpr double IDW_RAIO_M   = 100.0;
constexpr double IDW_DMIN_M   = 0.5;

class LogicaHierarquica {
public:
    explicit LogicaHierarquica(const Kml& kml,
                               double potencia = IDW_POTENCIA,
                               double raio_m   = IDW_RAIO_M,
                               double d_min_m  = IDW_DMIN_M);

    double dose(double x, double y) const;

private:
    bool   idw(double x, double y, double& valor) const;

    const Kml& kml_;
    double potencia_;
    double raio_m_;
    double d_min_m_;
};

}  // namespace vra
