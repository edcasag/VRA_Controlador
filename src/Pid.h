// PID enxuto, header-only. Anti-windup por clamping do termo integral.
#pragma once

#include <algorithm>

namespace vra {

// libstdc++ do toolchain Xtensa-ESP32 (GCC 8.4) nao expoe std::clamp.
inline double clamp_d(double v, double lo, double hi) {
    return std::min(std::max(v, lo), hi);
}

class Pid {
public:
    Pid(double kp, double ki, double kd,
        double dt, double saida_min, double saida_max)
        : kp_(kp), ki_(ki), kd_(kd),
          dt_(dt), saida_min_(saida_min), saida_max_(saida_max) {}

    void resetar() { integ_ = 0.0; erro_anterior_ = 0.0; }

    double atualizar(double setpoint, double leitura) {
        const double erro  = setpoint - leitura;
        const double p     = kp_ * erro;
        integ_            += ki_ * erro * dt_;
        integ_             = clamp_d(integ_, saida_min_, saida_max_);
        const double d     = kd_ * (erro - erro_anterior_) / dt_;
        erro_anterior_     = erro;
        return clamp_d(p + integ_ + d, saida_min_, saida_max_);
    }

private:
    double kp_, ki_, kd_;
    double dt_;
    double saida_min_, saida_max_;
    double integ_         = 0.0;
    double erro_anterior_ = 0.0;
};

}  // namespace vra
