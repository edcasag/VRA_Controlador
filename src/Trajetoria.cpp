#include "Trajetoria.h"

#include <algorithm>
#include <cmath>

namespace vra {

Trajetoria::Trajetoria(const Kml& kml, const Terreno& terreno,
                       double width_m, double step_m)
    : kml_(&kml), terreno_(&terreno),
      width_m_(width_m), step_m_(step_m),
      direction_(+1), t_acumulado_(0.0),
      terminou_(false), n_fixes_(0) {
    calcularBbox();
    const double margin = width_m_ * 0.5;
    y_atual_ = ymin_ + margin;
    x_atual_ = xmin_;  // primeiro fixe arrancara por aqui (filtrado pelo talhao)
}

void Trajetoria::calcularBbox() {
    xmin_ = ymin_ = +1e18;
    xmax_ = ymax_ = -1e18;
    if (!kml_->talhaoCarregado()) {
        terminou_ = true;
        return;
    }
    for (const auto& v : kml_->talhao().vertices) {
        xmin_ = std::min(xmin_, v.x);
        xmax_ = std::max(xmax_, v.x);
        ymin_ = std::min(ymin_, v.y);
        ymax_ = std::max(ymax_, v.y);
    }
}

bool Trajetoria::dentroDoTalhao(double x, double y) const {
    if (!kml_->talhaoCarregado()) return false;
    return pointInPolygon(x, y, kml_->talhao().vertices);
}

bool Trajetoria::dentroDeExclusao(double x, double y) const {
    // Poligonos com dose==0 (Sede etc.).
    for (const auto& r : kml_->regioesPoligonais()) {
        if (r.dose == 0.0 && pointInPolygon(x, y, r.vertices)) return true;
    }
    // Circulos com dose==0.
    for (const auto& c : kml_->regioesCirculares()) {
        if (c.dose == 0.0) {
            const double dx = x - c.centro.x;
            const double dy = y - c.centro.y;
            if (dx * dx + dy * dy <= c.raio_m * c.raio_m) return true;
        }
    }
    return false;
}

bool Trajetoria::proximoFixe(Fixe& out) {
    if (terminou_) return false;
    const double margin = width_m_ * 0.5;

    // Ate encontrar uma posicao valida ou esgotar todas as faixas.
    while (!terminou_) {
        // Ponto candidato na faixa atual, na direcao atual.
        // x_atual representa onde estamos (ja sera reportado depois de
        // avancar por step_m).
        const double x = x_atual_;
        const double y = y_atual_;

        // Saimos da faixa quando passamos do limite no eixo x?
        const bool saiu_da_faixa =
            (direction_ == +1 && x > xmax_) ||
            (direction_ == -1 && x < xmin_);

        if (saiu_da_faixa) {
            // Sobe para a proxima faixa, inverte direcao.
            y_atual_ += width_m_;
            direction_ = -direction_;
            x_atual_ = (direction_ == +1) ? xmin_ : xmax_;
            // Terminou quando passou do bbox em y.
            if (y_atual_ > ymax_ - margin + 1e-9) {
                terminou_ = true;
                return false;
            }
            continue;
        }

        // Avanca o cursor para o proximo passo (apos avaliar este).
        x_atual_ += direction_ * step_m_;

        // Se este ponto nao esta no talhao, pula sem emitir nem consumir tempo.
        if (!dentroDoTalhao(x, y)) {
            continue;
        }

        // Heading unitario na direcao atual (E-O).
        const double hx = static_cast<double>(direction_);
        const double hy = 0.0;
        const double v = velocidade(x, y, hx, hy, *terreno_);

        // dt = step_m / v (mesma formula do Python).
        const double dt = step_m_ / std::max(v, 1e-9);
        t_acumulado_ += dt;

        out.x = x;
        out.y = y;
        out.t = t_acumulado_;
        out.vel = v;
        out.heading_x = hx;
        out.heading_y = hy;
        // spreading = false se atravessamos exclusao.
        out.spreading = !dentroDeExclusao(x, y);
        ++n_fixes_;
        return true;
    }
    return false;
}

}  // namespace vra
