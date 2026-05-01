// Boustrofedon simplificado portado de _python/src/tractor_sim.py para C++.
// Cobre QUALQUER KML do LittleFS; perfeito em retangulos (ensaio_abcd,
// talhao_completo); perde ~5-10% de cobertura em quinas de poligonos
// irregulares (Sitio Palmar) por usar clipping ponto-a-ponto via
// pointInPolygon em vez de offset_polygon analitico.
//
// Sem cabeceira (headland_pass) nem offset_polygon — esses ficam para v1.1.
// Sem ruido GPS (gnss_noise=0) para garantir reprodutibilidade Python<->ESP32.
#pragma once

#include "Kml.h"
#include "Terreno.h"

namespace vra {

struct Fixe {
    double x;            // m (projetado)
    double y;            // m (projetado)
    double t;            // s (tempo simulado acumulado)
    double vel;          // m/s
    double heading_x;    // vetor unitario (componente x)
    double heading_y;    // vetor unitario (componente y)
    bool   spreading;    // false se atravessando exclusao
};

class Trajetoria {
public:
    Trajetoria(const Kml& kml, const Terreno& terreno,
               double width_m = 20.0, double step_m = 1.0);

    // Retorna false quando todas as faixas foram cobertas.
    bool proximoFixe(Fixe& out);

    // Total acumulado (estimativa para barra de progresso).
    long fixesEmitidos() const { return n_fixes_; }
    bool terminou()      const { return terminou_; }

private:
    const Kml*     kml_;
    const Terreno* terreno_;
    double width_m_, step_m_;
    double xmin_, ymin_, xmax_, ymax_;
    double y_atual_;
    double x_atual_;
    int    direction_;     // +1 = O->L, -1 = L->O
    double t_acumulado_;
    bool   terminou_;
    long   n_fixes_;

    void calcularBbox();
    bool dentroDoTalhao(double x, double y) const;
    bool dentroDeExclusao(double x, double y) const;
};

}  // namespace vra
