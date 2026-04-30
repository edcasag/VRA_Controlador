// Modelo de planta de 1a ordem para o atuador (servo de abertura).
// Usado no BUILD_SIM para simular a inercia mecanica entre a saida do
// PID (PWM -100..+100) e a posicao percentual real do servo (0..100%).
//
// Equacao: pos += (saida * GANHO - pos) * (dt / TAU), saturada em [0, 100].
// Constante de tempo TAU=0.3s tipica de servos comerciais; GANHO=1.0 porque
// a saida do PID ja esta normalizada em escala de %.
#pragma once

#include <algorithm>

namespace vra {

class Planta {
public:
    static constexpr double TAU = 0.3;     // s
    static constexpr double GANHO = 1.0;   // saida do PID ja esta em %

    explicit Planta(double pos_inicial = 0.0)
        : pos_pct_(std::max(0.0, std::min(100.0, pos_inicial))) {}

    // Avança o estado da planta por dt segundos com a saida atual do PID.
    void passo(double saida_pwm, double dt) {
        const double alvo = saida_pwm * GANHO;
        pos_pct_ += (alvo - pos_pct_) * (dt / TAU);
        pos_pct_ = std::max(0.0, std::min(100.0, pos_pct_));
    }

    double posicao() const { return pos_pct_; }
    void resetar(double pos = 0.0) {
        pos_pct_ = std::max(0.0, std::min(100.0, pos));
    }

private:
    double pos_pct_;
};

}  // namespace vra
