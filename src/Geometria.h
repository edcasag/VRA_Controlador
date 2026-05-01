// Geometria 2D para o POC VRA_Controlador.
// Termos: Coord (lat/lon graus), Ponto (x/y metros — projeção equiretangular
// local, origem fixada pelo KML carregado, conforme artigo SBIAGRO 2025).
#pragma once

#include <cmath>
#include <vector>

namespace vra {

constexpr double EARTH_RADIUS_M  = 6371000.0;
// Arduino.h define DEG_TO_RAD como macro -> nome alternativo aqui.
constexpr double GRAUS_PARA_RAD  = 0.017453292519943295;  // M_PI / 180.0

struct Coord { double lat; double lon; };
struct Ponto { double x;   double y;   };

inline Ponto projetar(Coord c, Coord origem) {
    const double lat0_rad = origem.lat * GRAUS_PARA_RAD;
    return Ponto{
        (c.lon - origem.lon) * GRAUS_PARA_RAD * std::cos(lat0_rad) * EARTH_RADIUS_M,
        (c.lat - origem.lat) * GRAUS_PARA_RAD * EARTH_RADIUS_M
    };
}

inline double polygonArea(const std::vector<Ponto>& poly) {
    const size_t n = poly.size();
    if (n < 3) return 0.0;
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Ponto& a = poly[i];
        const Ponto& b = poly[(i + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    return std::fabs(s) * 0.5;
}

// Ray casting clássico. O termo 1e-30 evita divisão por zero quando uma
// aresta é horizontal (yj == yi) — espelha o Python (vra_engine.py).
inline bool pointInPolygon(double x, double y, const std::vector<Ponto>& poly) {
    const size_t n = poly.size();
    if (n < 3) return false;
    bool dentro = false;
    size_t j = n - 1;
    for (size_t i = 0; i < n; ++i) {
        const double xi = poly[i].x, yi = poly[i].y;
        const double xj = poly[j].x, yj = poly[j].y;
        if ((yi > y) != (yj > y)) {
            const double xint = (xj - xi) * (y - yi) / (yj - yi + 1e-30) + xi;
            if (x < xint) dentro = !dentro;
        }
        j = i;
    }
    return dentro;
}

}  // namespace vra
