// Modelo de terreno para perfil de velocidade variavel.
// Espelha 1:1 _python/src/terrain.py (declive linear + bossas Gaussianas).
// O ESP32 usa este modelo no BUILD_SIM para gerar a velocidade ponto-a-ponto
// na trajetoria boustrofedon, da mesma forma que o tractor_sim Python.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace vra {

struct Bossa {
    double h;      // altura (m)
    double x0;     // centro x (m, projetado)
    double y0;     // centro y (m, projetado)
    double sigma;  // raio caracteristico (m)
};

struct Terreno {
    double a = 0.04;     // declive em x (m/m), default 4% E->O
    double b = 0.0;      // declive em y (m/m)
    std::vector<Bossa> bossas;
    double v_nom = 5.0;  // m/s, velocidade nominal em terreno plano
    double v_min = 1.5;  // m/s, saturacao minima
    double v_max = 7.0;  // m/s, saturacao maxima
    double alpha = 50.0; // m/s por (m/m), sensibilidade ao gradiente
};

// Espelha terrain.default_params() Python: declive 4% E->O, 1 bossa central
// com h=2.0 m e sigma = 20% do lado maior do bbox.
inline Terreno terrenoDefault(double xmin, double ymin, double xmax, double ymax) {
    Terreno t;
    t.a = 0.04;
    t.b = 0.0;
    const double cx = 0.5 * (xmin + xmax);
    const double cy = 0.5 * (ymin + ymax);
    const double sigma = 0.20 * std::max(xmax - xmin, ymax - ymin);
    t.bossas.push_back({2.0, cx, cy, sigma});
    return t;
}

inline double altitude(double x, double y, const Terreno& p) {
    double z = p.a * x + p.b * y;
    for (const auto& b : p.bossas) {
        const double dx = x - b.x0;
        const double dy = y - b.y0;
        z += b.h * std::exp(-(dx * dx + dy * dy) / (2.0 * b.sigma * b.sigma));
    }
    return z;
}

inline void gradiente(double x, double y, const Terreno& p, double& gx, double& gy) {
    gx = p.a;
    gy = p.b;
    for (const auto& b : p.bossas) {
        const double dx = x - b.x0;
        const double dy = y - b.y0;
        const double s2 = b.sigma * b.sigma;
        const double e = std::exp(-(dx * dx + dy * dy) / (2.0 * s2));
        gx += b.h * e * (-dx / s2);
        gy += b.h * e * (-dy / s2);
    }
}

// Velocidade em (x,y) movendo-se na direcao 'heading' (vetor, normalizado internamente).
inline double velocidade(double x, double y, double hx, double hy, const Terreno& p) {
    double gx, gy;
    gradiente(x, y, p, gx, gy);
    const double norm = std::sqrt(hx * hx + hy * hy);
    if (norm < 1e-12) return p.v_nom;
    hx /= norm;
    hy /= norm;
    const double declive_along = gx * hx + gy * hy;
    const double v = p.v_nom - p.alpha * declive_along;
    return std::max(p.v_min, std::min(p.v_max, v));
}

}  // namespace vra
