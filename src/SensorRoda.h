// Sensor de velocidade do trator por pulsos na roda. Conta o tempo entre bordas
// de subida em um pino digital (GPIO 19, conforme esquema EC-1.0 REV02). A
// velocidade é puramente geométrica: depende do diâmetro do pneu e do número
// de ímãs/parafusos por revolução. Sem calibração de campo necessária.
//
// Trator parado (ausência de pulsos por mais de TIMEOUT_MS) => vel_kmh = 0,
// o que faz a abertura do atuador fechar via posicao_alvo_para_taxa.
//
// Adaptado da máquina de estados de Sensor::detectVelTrator() do SyncatV4-MINI,
// simplificada porque a taxa de pulsos da roda do trator é baixa (max ~5 Hz a
// 12 km/h com 4 ímãs por revolução) — o debounce por período mínimo é
// suficiente, sem precisar de máquina de estados de 5 fases.
#pragma once

#include <Arduino.h>

namespace vra {

class SensorRoda {
public:
    // Parâmetros geométricos. Editar conforme o trator/roda específicos.
    // Configuração do ensaio integrado: pneu dianteiro aro 24 (~1000 mm de
    // diâmetro externo) e 8 parafusos por revolução servindo de fonte de pulso
    // para o sensor magnético.
    static constexpr double D_PNEU_MM    = 1000.0;  // diâmetro do pneu dianteiro (mm)
    static constexpr int    N_PULSOS_REV = 8;       // parafusos da roda dianteira

    // K_PNEU = D_PNEU[mm] * π * 3.6 / N_PULSOS_REV  (km·ms/h)
    // vel_kmh = K_PNEU / period_ms
    static constexpr double K_PNEU = D_PNEU_MM * 11.30973 / static_cast<double>(N_PULSOS_REV);

    // Período mínimo entre pulsos válidos (debounce). Com 8 parafusos e roda
    // de 1000 mm, ~8 pulsos/s a 12 km/h (período ~125 ms). MIN_PERIODO_MS = 30
    // ms dá folga sem cortar pulsos reais.
    static constexpr unsigned long MIN_PERIODO_MS = 30;

    // Sem pulsos por mais que isso => trator parado.
    static constexpr unsigned long TIMEOUT_MS = 1000;

    void iniciar(int pino) {
        pino_ = pino;
        pinMode(pino_, INPUT);
        last_state_     = digitalRead(pino_);
        last_pulse_ms_  = 0;
        periodo_ms_     = 0;
    }

    // Chamado a cada tick da task de polling (tipicamente 20 ms, vindo de
    // task_controle do main_poc).
    void tick(unsigned long agora_ms) {
        const int state = digitalRead(pino_);
        // Detecção de borda de subida (0 -> 1).
        if (state == 1 && last_state_ == 0) {
            const unsigned long dt = agora_ms - last_pulse_ms_;
            if (dt > MIN_PERIODO_MS) {
                periodo_ms_    = dt;
                last_pulse_ms_ = agora_ms;
            }
            // Se dt <= MIN_PERIODO_MS, é ruído/debounce, ignora.
        }
        last_state_ = state;

        // Timeout: sem pulsos há muito tempo => parado.
        if (agora_ms - last_pulse_ms_ > TIMEOUT_MS) {
            periodo_ms_ = 0;
        }
    }

    // Velocidade calculada. Zero quando parado ou em timeout.
    double vel_kmh() const {
        if (periodo_ms_ == 0) return 0.0;
        return K_PNEU / static_cast<double>(periodo_ms_);
    }

    // Para diagnóstico/log.
    unsigned long periodo_ms() const { return periodo_ms_; }

private:
    int pino_                       = -1;
    int last_state_                 = 0;
    unsigned long last_pulse_ms_    = 0;
    unsigned long periodo_ms_       = 0;  // 0 = parado / sem pulso
};

}  // namespace vra
