#include "LogicaHierarquica.h"

#include <cmath>

namespace vra {

LogicaHierarquica::LogicaHierarquica(const Kml& kml,
                                     double potencia,
                                     double raio_m,
                                     double d_min_m)
    : kml_(kml),
      potencia_(potencia),
      raio_m_(raio_m),
      d_min_m_(d_min_m) {}

bool LogicaHierarquica::idw(double x, double y, double& valor) const {
    const auto& pts = kml_.pontosReferencia();
    if (pts.empty()) return false;
    const double raio_sq = raio_m_ * raio_m_;
    double soma_pesos    = 0.0;
    double soma_pond     = 0.0;
    for (const auto& p : pts) {
        const double dx   = x - p.pos.x;
        const double dy   = y - p.pos.y;
        const double d_sq = dx * dx + dy * dy;
        if (d_sq > raio_sq) continue;
        const double d = std::fmax(std::sqrt(d_sq), d_min_m_);
        const double w = 1.0 / std::pow(d, potencia_);
        soma_pond  += w * p.dose;
        soma_pesos += w;
    }
    if (soma_pesos == 0.0) return false;
    valor = soma_pond / soma_pesos;
    return true;
}

double LogicaHierarquica::dose(double x, double y) const {
    // 1. Regiões circulares: a primeira que contém o ponto vence (espelha Python).
    for (const auto& c : kml_.regioesCirculares()) {
        const double dx = x - c.centro.x;
        const double dy = y - c.centro.y;
        if (dx * dx + dy * dy <= c.raio_m * c.raio_m) return c.dose;
    }

    // 2. Polígonos de exclusão (dose==0) prevalecem sobre inclusão.
    for (const auto& r : kml_.regioesPoligonais()) {
        if (r.dose == 0.0 && pointInPolygon(x, y, r.vertices)) return 0.0;
    }

    // 3. Polígonos de inclusão: vence o de menor área (mais específico).
    bool   achou         = false;
    double menor_area    = 0.0;
    double dose_da_menor = 0.0;
    for (const auto& r : kml_.regioesPoligonais()) {
        if (r.dose > 0.0 && pointInPolygon(x, y, r.vertices)) {
            if (!achou || r.area_m2 < menor_area) {
                menor_area    = r.area_m2;
                dose_da_menor = r.dose;
                achou         = true;
            }
        }
    }
    if (achou) return dose_da_menor;

    // 4. IDW por pontos de referência.
    double idw_valor = 0.0;
    if (idw(x, y, idw_valor)) return idw_valor;

    // 5-6. Dose-base se dentro do talhão; senão zero.
    if (kml_.talhaoCarregado() && pointInPolygon(x, y, kml_.talhao().vertices)) {
        return kml_.talhao().dose_base;
    }
    return 0.0;
}

}  // namespace vra
