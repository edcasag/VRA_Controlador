// Modo BUILD_SIM — simulacao completa standalone do POC VRA_Controlador
// num ESP32 DevKit V1, sem nenhum periferico externo (so USB para Serial).
//
// Encadeia o ciclo completo: trajetoria GPS sintetica (boustrofedon com
// velocidade variavel por declive) -> LogicaHierarquica -> PID em malha
// fechada -> atuador simulado com inercia (Planta de 1a ordem) ->
// agregacao de erro por zona -> relatorio final estilo coverage_report.py
// do VRA_Simulador Python.
//
// No boot, lista todos os *.kml em / do LittleFS e pede para o usuario
// escolher. Auto-detecta CSV ground truth (trajetoria_<base>.csv) para
// validacao cruzada Python<->ESP32.
//
// Selecionado via build_flags em platformio.ini: -DBUILD_SIM.
// Idioma das mensagens: -DLANG_PT (default) ou -DLANG_EN.
#ifdef BUILD_SIM
#pragma message "Compilando modo: BUILD_SIM"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include <cmath>
#include <cstdio>

#include "Atuador.h"
#include "CsvReader.h"
#include "Geometria.h"
#include "Kml.h"
#include "LogicaHierarquica.h"
#include "MenuSerial.h"
#include "Pid.h"
#include "Planta.h"
#include "Relatorio.h"
#include "Terreno.h"
#include "Trajetoria.h"
#include "i18n.h"

namespace {

constexpr double LARGURA_M = vra::LARGURA_M;     // 20.0
constexpr double STEP_M    = 1.0;
constexpr double DT_PID    = 0.02;               // 50 Hz
constexpr int    PONTINHO_INTERVALO = 50;        // 1 ponto a cada 50 fixes

void calcularBbox(const vra::Talhao& t, double& xmin, double& ymin,
                  double& xmax, double& ymax) {
    xmin = ymin = +1e18;
    xmax = ymax = -1e18;
    for (const auto& v : t.vertices) {
        if (v.x < xmin) xmin = v.x;
        if (v.x > xmax) xmax = v.x;
        if (v.y < ymin) ymin = v.y;
        if (v.y > ymax) ymax = v.y;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println();
    Serial.println(F(MSG_BANNER));

    if (!LittleFS.begin(true)) {
        Serial.println(F(MSG_MOUNT_FAIL));
        return;
    }

    // ---- Menu Serial: lista *.kml em / e pede escolha ----
    vra::MenuSerial menu;
    const int n_kmls = menu.descobrir();
    if (n_kmls == 0) {
        Serial.println(F(MSG_NO_KMLS));
        return;
    }
    menu.exibir(Serial);
    const int idx = menu.lerEscolha(Serial, Serial);
    const auto& opcao = menu.opcoes()[idx];

    // ---- Carrega o KML escolhido ----
    vra::Kml kml;
    if (!kml.carregarDoArquivo(opcao.caminho.c_str())) {
        char buf[120];
        std::snprintf(buf, sizeof(buf), MSG_LOADED_KML_FAIL, opcao.caminho.c_str());
        Serial.println(buf);
        return;
    }
    {
        char buf[120];
        std::snprintf(buf, sizeof(buf), MSG_LOADED_KML, opcao.caminho.c_str(),
                      static_cast<int>(kml.regioesPoligonais().size()));
        Serial.println(buf);
    }

    // ---- Bbox do talhao -> Terreno default (declive 4% E->O + 1 bossa) ----
    if (!kml.talhaoCarregado()) {
        Serial.println(F("[ERRO] KML sem Talhao (Field=)"));
        return;
    }
    double xmin, ymin, xmax, ymax;
    calcularBbox(kml.talhao(), xmin, ymin, xmax, ymax);
    const vra::Terreno terreno = vra::terrenoDefault(xmin, ymin, xmax, ymax);

    // ---- Trajetoria boustrofedon (port C++ simplificado) ----
    vra::Trajetoria trajetoria(kml, terreno, LARGURA_M, STEP_M);

    // ---- CSV ground truth opcional ----
    vra::CsvReader csv;
    bool tem_csv_python = false;
    if (opcao.tem_csv && csv.abrir(opcao.csv_caminho.c_str())) {
        tem_csv_python = csv.tem_dose_python();
        char buf[160];
        // Conta as linhas do CSV: usaremos contagem dinamica conforme lemos.
        std::snprintf(buf, sizeof(buf), MSG_LOADED_CSV, opcao.csv_caminho.c_str(), 0);
        Serial.println(buf);
    } else {
        Serial.println(F(MSG_NO_CSV));
    }

    // ---- Componentes do laco de controle ----
    vra::LogicaHierarquica logica(kml);
    // Mesmos gains do main_poc.cpp da etapa 3, mas dt = 50 Hz para o SIM.
    vra::Pid pid(8.0, 1.5, 0.0, DT_PID, -100.0, 100.0);
    vra::Planta planta;
    vra::Relatorio relatorio;

    Serial.print(F(MSG_SIM_STARTING));

    // ---- Loop principal ----
    vra::Fixe fixe;
    double soma_lat_dose = 0.0;
    long   n_lat_dose    = 0;
    double soma_lat_pid  = 0.0;
    long   n_lat_pid     = 0;
    long   n_fixes       = 0;
    double t_anterior    = 0.0;

    while (trajetoria.proximoFixe(fixe)) {
        // 1) Logica hierarquica -> dose alvo do ESP32 (cronometrada).
        const unsigned long t0 = micros();
        const double dose_esp = logica.dose(fixe.x, fixe.y);
        soma_lat_dose += static_cast<double>(micros() - t0);
        ++n_lat_dose;

        // 2) Comparacao com dose Python (se CSV presente).
        if (tem_csv_python) {
            vra::LinhaCsv linha;
            if (csv.proximaLinha(linha)) {
                relatorio.compararPython(dose_esp, linha.dose_python);
            }
        }

        // 3) Setpoint do atuador.
        const double vel_kmh = fixe.vel * 3.6;
        const vra::AlvoAtuador alvo = vra::posicao_alvo_para_taxa(
            fixe.spreading ? dose_esp : 0.0, vel_kmh);

        // 4) Quantos passos do PID rodam ate o proximo fixe.
        const double dt_fixe = fixe.t - t_anterior;
        t_anterior = fixe.t;
        int n_passos = static_cast<int>(dt_fixe / DT_PID + 0.5);
        if (n_passos < 1) n_passos = 1;
        if (n_passos > 50) n_passos = 50;  // proteçao em fixes longos

        const unsigned long tp0 = micros();
        for (int k = 0; k < n_passos; ++k) {
            const double saida = pid.atualizar(alvo.posicao_pct, planta.posicao());
            planta.passo(saida, DT_PID);
        }
        soma_lat_pid += static_cast<double>(micros() - tp0) / n_passos;
        ++n_lat_pid;

        // 5) Dose efetivamente aplicada considerando o estado real do servo.
        double dose_efetiva = 0.0;
        if (fixe.spreading && vel_kmh >= vra::V_MIN_OPERACAO_KMH) {
            const double abertura = vra::abertura_em(planta.posicao());
            // dose [kg/ha] = vazao[kg/min] * 600 / (vel[km/h] * larg[m])
            const double vazao = abertura * vra::FATOR_ESCOAMENTO_KG_MIN;
            dose_efetiva = vazao * 600.0 / (vel_kmh * LARGURA_M);
        }

        relatorio.acumular(dose_esp, dose_efetiva, fixe.vel, LARGURA_M, STEP_M);

        // 6) Pontinho de progresso.
        if (++n_fixes % PONTINHO_INTERVALO == 0) {
            Serial.print('.');
        }
    }

    Serial.println(F(MSG_SIM_DONE));
    csv.fechar();

    // ---- Latencias agregadas ----
    const double lat_dose_us = (n_lat_dose > 0) ? soma_lat_dose / n_lat_dose : 0.0;
    const double lat_pid_us  = (n_lat_pid  > 0) ? soma_lat_pid  / n_lat_pid  : 0.0;
    relatorio.registrarLatencias(lat_dose_us, lat_pid_us);

    // ---- Imprime relatorio final ----
    relatorio.imprimir(Serial);
}

void loop() {
    delay(60000);
}

#endif  // BUILD_SIM
