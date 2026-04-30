// Modo BUILD_POC — sistema completo. GPS UBlox M8N em UART2 (NMEA), atuador
// linear DC com PWM avançar/recuar + feedback ADC potenciométrico, PID em
// posição, botões físicos START/STOP, 4 modos: EXEC, CAL1, CAL2, CAL3.
// Selecionado via build_flags em platformio.ini: -DBUILD_POC.
//
// Arquitetura FreeRTOS:
//   task_logica  (core 0, 2 Hz):  GPS -> projeção -> dose -> vazão -> posicao_alvo
//   task_controle(core 1, 50 Hz): ADC -> PID -> PWM
//   loop() Arduino:               drena fila_log, lê botões, parse comandos Serial
//
// Calibrações cal1->cal2->cal3 rodam blocking dentro do loop(); durante elas a
// task_controle é desativada (modo_ctrl = CTRL_IDLE) e a task_logica não atualiza
// o setpoint.
#ifdef BUILD_POC
#pragma message "Compilando modo: BUILD_POC"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Atuador.h"
#include "Kml.h"
#include "LogicaHierarquica.h"
#include "Pid.h"

namespace {

// ---------------------------------------------------------------------------
// Pinout (Cap. 6 §6.7 da tese / plano).
// ---------------------------------------------------------------------------
constexpr int PIN_PWM_AVANCAR  = 25;
constexpr int PIN_PWM_RECUAR   = 26;
constexpr int PIN_FEEDBACK_ADC = 34;
constexpr int PIN_BTN_START    = 32;
constexpr int PIN_BTN_STOP     = 33;
constexpr int PIN_GPS_RX       = 16;  // UART2 RX <- GPS TX
constexpr int PIN_GPS_TX       = 17;  // UART2 TX -> GPS RX (não usado)

constexpr int PWM_FREQ_HZ = 1000;
constexpr int PWM_RES     = 8;       // 8-bit -> 0..255

// Canais LEDC (Arduino-ESP32 2.0.x usa API ledcSetup/ledcAttachPin/ledcWrite
// com canal; a API simplificada ledcAttach(pin, freq, res) eh do 3.x).
constexpr int LEDC_CH_AVANCAR = 0;
constexpr int LEDC_CH_RECUAR  = 1;

// Extremos do potenciômetro (calibrar com cal1 e atualizar aqui).
constexpr double V_MIN = 0.50;       // V no atuador 0% (recuado)
constexpr double V_MAX = 2.70;       // V no atuador 100% (avançado)

// PID gains conservadores (tuning no campo).
constexpr double KP = 8.0, KI = 1.5, KD = 0.0;
constexpr double DT_PID = 0.02;      // 50 Hz

constexpr const char* CAMINHO_KML = "/Sitio_Palmar.kml";

// ---------------------------------------------------------------------------
// Estado compartilhado (32-bit -> leitura/escrita atômica no ESP32).
// ---------------------------------------------------------------------------
enum ModoCtrl : uint8_t  { CTRL_IDLE = 0, CTRL_PID, CTRL_MANUAL_AV, CTRL_MANUAL_REC };
enum ModoApp  : uint8_t  { MODO_EXEC = 0, MODO_CAL1, MODO_CAL2, MODO_CAL3 };

volatile float    posicao_alvo_pct = 0.0f;
volatile uint8_t  modo_ctrl        = CTRL_IDLE;
volatile uint8_t  modo_app         = MODO_EXEC;
volatile bool     ativo            = false;

struct MensagemLog { char texto[128]; };
QueueHandle_t fila_log = nullptr;

vra::Kml*               g_kml    = nullptr;
vra::LogicaHierarquica* g_logica = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void log_msg(const char* fmt, ...) {
    if (!fila_log) return;
    MensagemLog m;
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(m.texto, sizeof(m.texto), fmt, args);
    va_end(args);
    xQueueSend(fila_log, &m, 0);  // descarta se cheia
}

double ler_volts_pot() {
    const int adc = analogRead(PIN_FEEDBACK_ADC);
    return (adc / 4095.0) * 3.3;
}

double posicao_atual_pct() {
    const double v = ler_volts_pot();
    double pct = (v - V_MIN) / (V_MAX - V_MIN) * 100.0;
    if (pct < 0.0)   pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    return pct;
}

void pwm_avancar(int duty_0_255) {
    ledcWrite(LEDC_CH_AVANCAR, duty_0_255);
    ledcWrite(LEDC_CH_RECUAR,  0);
}
void pwm_recuar(int duty_0_255) {
    ledcWrite(LEDC_CH_RECUAR,  duty_0_255);
    ledcWrite(LEDC_CH_AVANCAR, 0);
}
void pwm_zerar() {
    ledcWrite(LEDC_CH_AVANCAR, 0);
    ledcWrite(LEDC_CH_RECUAR,  0);
}

// ---------------------------------------------------------------------------
// NMEA parser não-bloqueante. Lê de Serial2 char-a-char. Suporta $GNRMC e
// $GPRMC. Em parado, o GPS reporta vel ~0 (com leve ruído), por isso o filtro
// V_MIN_OPERACAO_KMH no posicao_alvo_para_taxa().
// ---------------------------------------------------------------------------
struct Fix { bool valid = false; double lat = 0; double lon = 0; double vel_kmh = 0; };

double nmea_lat_lon_decimal(const char* tok, char hemis) {
    const double raw  = std::atof(tok);
    const double deg  = std::floor(raw / 100.0);
    const double mins = raw - deg * 100.0;
    double dec        = deg + mins / 60.0;
    if (hemis == 'S' || hemis == 'W') dec = -dec;
    return dec;
}

bool parse_rmc(const char* linha, Fix& fix) {
    if (std::strncmp(linha, "$GNRMC", 6) != 0 &&
        std::strncmp(linha, "$GPRMC", 6) != 0) return false;
    char buf[120];
    std::strncpy(buf, linha, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tokens[13] = {};
    int n = 0;
    char* save = nullptr;
    // strtok_r eh POSIX (declarada em <string.h>), nao std::.
    for (char* p = strtok_r(buf, ",", &save); p && n < 13;
         p = strtok_r(nullptr, ",", &save)) {
        tokens[n++] = p;
    }
    if (n < 8) return false;
    if (!tokens[2] || tokens[2][0] != 'A') return false;        // status
    if (!tokens[3] || !tokens[4] || !tokens[5] || !tokens[6]) return false;

    fix.lat     = nmea_lat_lon_decimal(tokens[3], tokens[4][0]);
    fix.lon     = nmea_lat_lon_decimal(tokens[5], tokens[6][0]);
    fix.vel_kmh = (tokens[7] ? std::atof(tokens[7]) : 0.0) * 1.852;
    fix.valid   = true;
    return true;
}

class LeitorNmea {
public:
    bool ler(Fix& fix) {
        while (Serial2.available()) {
            const char c = static_cast<char>(Serial2.read());
            if (c == '\r') continue;
            if (c == '\n') {
                buf_[len_] = '\0';
                const bool ok = parse_rmc(buf_, fix);
                len_ = 0;
                if (ok) return true;
            } else if (len_ < sizeof(buf_) - 1) {
                buf_[len_++] = c;
            } else {
                len_ = 0;  // overflow -> descarta
            }
        }
        return false;
    }
private:
    char buf_[120] {};
    size_t len_ = 0;
};

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------
void task_controle(void*) {
    vra::Pid pid(KP, KI, KD, DT_PID, -100.0, 100.0);
    const TickType_t periodo = pdMS_TO_TICKS(20);
    TickType_t t_anterior = xTaskGetTickCount();

    for (;;) {
        const uint8_t mc = modo_ctrl;
        if (mc == CTRL_PID) {
            const double pos    = posicao_atual_pct();
            const double saida  = pid.atualizar(static_cast<double>(posicao_alvo_pct), pos);
            const int duty      = static_cast<int>(std::fabs(saida) * 2.55);
            if (saida > 0) pwm_avancar(duty); else pwm_recuar(duty);
        } else if (mc == CTRL_MANUAL_AV) {
            pwm_avancar(255); pid.resetar();
        } else if (mc == CTRL_MANUAL_REC) {
            pwm_recuar(255); pid.resetar();
        } else {
            pwm_zerar(); pid.resetar();
        }
        vTaskDelayUntil(&t_anterior, periodo);
    }
}

void task_logica(void*) {
    LeitorNmea nmea;
    const TickType_t periodo = pdMS_TO_TICKS(500);
    TickType_t t_anterior = xTaskGetTickCount();

    for (;;) {
        const uint8_t ma = modo_app;
        Fix fix;
        const bool tem_fix = nmea.ler(fix);

        if (ma == MODO_EXEC && ativo && tem_fix && g_kml && g_logica) {
            const auto p = vra::projetar(vra::Coord{fix.lat, fix.lon}, g_kml->origem());
            const unsigned long t0 = micros();
            const double rate = g_logica->dose(p.x, p.y);
            const auto alvo = vra::posicao_alvo_para_taxa(rate, fix.vel_kmh);
            const long dt_us = micros() - t0;

            posicao_alvo_pct = static_cast<float>(alvo.posicao_pct);
            modo_ctrl        = CTRL_PID;

            log_msg("lat=%.6f lon=%.6f v=%.1f km/h | rate=%.0f kg/ha | "
                    "alvo=%.1f%% real=%.1f%% | %ld us%s",
                    fix.lat, fix.lon, fix.vel_kmh, rate,
                    alvo.posicao_pct, posicao_atual_pct(), dt_us,
                    alvo.saturou ? " [SAT]" : "");
        } else if (ma == MODO_EXEC && !ativo) {
            modo_ctrl        = CTRL_IDLE;
            posicao_alvo_pct = 0.0f;
        }
        vTaskDelayUntil(&t_anterior, periodo);
    }
}

// ---------------------------------------------------------------------------
// Calibrações (rodam blocking no loop, com modo_ctrl neutralizado).
// ---------------------------------------------------------------------------
void cal1() {
    log_msg("[CAL1] inicio. Recuando 10s...");
    modo_ctrl = CTRL_MANUAL_REC;
    vTaskDelay(pdMS_TO_TICKS(10000));
    modo_ctrl = CTRL_IDLE;
    vTaskDelay(pdMS_TO_TICKS(500));
    const double v_min_lido = ler_volts_pot();
    log_msg("[CAL1] V_MIN lido = %.3f V (atualizar V_MIN no codigo)", v_min_lido);

    log_msg("[CAL1] avancando 10s...");
    modo_ctrl = CTRL_MANUAL_AV;
    vTaskDelay(pdMS_TO_TICKS(10000));
    modo_ctrl = CTRL_IDLE;
    vTaskDelay(pdMS_TO_TICKS(500));
    const double v_max_lido = ler_volts_pot();
    log_msg("[CAL1] V_MAX lido = %.3f V (atualizar V_MAX no codigo)", v_max_lido);
    log_msg("[CAL1] FIM. Editar V_MIN/V_MAX em main_poc.cpp e recompilar.");
}

void cal2() {
    log_msg("[CAL2] inicio. Tabela posicao -> abertura (manual).");
    for (int pct = 0; pct <= 100; pct += 10) {
        posicao_alvo_pct = static_cast<float>(pct);
        modo_ctrl        = CTRL_PID;
        vTaskDelay(pdMS_TO_TICKS(5000));     // PID estabiliza
        modo_ctrl        = CTRL_IDLE;
        log_msg("[CAL2] em %d%% (real=%.1f%%): medir abertura, ENTER no Serial",
                pct, posicao_atual_pct());
        while (!Serial.available()) vTaskDelay(pdMS_TO_TICKS(50));
        while (Serial.available()) (void) Serial.read();
    }
    log_msg("[CAL2] FIM. Atualizar TABELA_ABERTURA em Atuador.h e recompilar.");
}

void cal3() {
    log_msg("[CAL3] inicio. Posicao 50%%; START -> escoar; STOP -> medir.");
    posicao_alvo_pct = 50.0f;
    modo_ctrl        = CTRL_PID;
    // Aguarda atingir a posição (até 5 s)
    const unsigned long t_inicio = millis();
    while (std::fabs(posicao_atual_pct() - 50.0) > 2.0 &&
           millis() - t_inicio < 5000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    log_msg("[CAL3] em 50%%, escoando produto. Aguardando STOP...");
    const unsigned long t_start_escoa = millis();
    while (digitalRead(PIN_BTN_STOP) == HIGH) vTaskDelay(pdMS_TO_TICKS(50));
    const unsigned long t_stop_escoa = millis();
    const double duracao_min = (t_stop_escoa - t_start_escoa) / 60000.0;

    posicao_alvo_pct = 0.0f;
    log_msg("[CAL3] duracao = %.2f min a 50%% (TABELA_ABERTURA[5] = %.3f).",
            duracao_min, vra::TABELA_ABERTURA[5].abertura);
    log_msg("[CAL3] Pesar (kg) e calcular: vazao_50 = kg / duracao_min;");
    log_msg("[CAL3] FATOR_ESCOAMENTO_KG_MIN = vazao_50 / TABELA_ABERTURA[5].abertura");
    log_msg("[CAL3] Atualizar em Atuador.h e recompilar.");
}

// ---------------------------------------------------------------------------
// Comandos Serial (m e | m c1 | m c2 | m c3) e botões físicos.
// ---------------------------------------------------------------------------
void processar_comandos_serial() {
    static char buf[16];
    static size_t len = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            buf[len] = '\0';
            if (std::strncmp(buf, "m e",  3) == 0) { modo_app = MODO_EXEC; ativo = false; log_msg("[modo] EXEC"); }
            else if (std::strncmp(buf, "m c1", 4) == 0) { modo_app = MODO_CAL1; log_msg("[modo] CAL1"); }
            else if (std::strncmp(buf, "m c2", 4) == 0) { modo_app = MODO_CAL2; log_msg("[modo] CAL2"); }
            else if (std::strncmp(buf, "m c3", 4) == 0) { modo_app = MODO_CAL3; log_msg("[modo] CAL3"); }
            len = 0;
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        } else {
            len = 0;
        }
    }
}

void processar_botoes() {
    static unsigned long t_debounce = 0;
    if (millis() - t_debounce < 50) return;
    if (digitalRead(PIN_BTN_START) == LOW && !ativo) { ativo = true;  t_debounce = millis(); log_msg("[btn] START"); }
    if (digitalRead(PIN_BTN_STOP)  == LOW &&  ativo) { ativo = false; t_debounce = millis(); log_msg("[btn] STOP"); }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup_hardware() {
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_BTN_STOP,  INPUT_PULLUP);
    analogReadResolution(12);
    ledcSetup(LEDC_CH_AVANCAR, PWM_FREQ_HZ, PWM_RES);
    ledcSetup(LEDC_CH_RECUAR,  PWM_FREQ_HZ, PWM_RES);
    ledcAttachPin(PIN_PWM_AVANCAR, LEDC_CH_AVANCAR);
    ledcAttachPin(PIN_PWM_RECUAR,  LEDC_CH_RECUAR);
    pwm_zerar();
    Serial2.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println();
    Serial.println(F("=== POC VRA_Controlador (env: poc) ==="));

    setup_hardware();

    if (!LittleFS.begin(true)) {
        Serial.println(F("[ERRO] LittleFS nao montou"));
        return;
    }
    static vra::Kml kml;
    if (!kml.carregarDoArquivo(CAMINHO_KML)) {
        Serial.printf("[ERRO] falha ao carregar %s\n", CAMINHO_KML);
        return;
    }
    static vra::LogicaHierarquica logica(kml);
    g_kml    = &kml;
    g_logica = &logica;
    kml.imprimirSumario(Serial);

    fila_log = xQueueCreate(16, sizeof(MensagemLog));
    xTaskCreatePinnedToCore(task_controle, "ctrl",   4096, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(task_logica,   "logica", 8192, nullptr, 3, nullptr, 0);

    Serial.println(F("Comandos: 'm e' EXEC | 'm c1' CAL1 | 'm c2' CAL2 | 'm c3' CAL3"));
    Serial.println(F("Botoes: GPIO32 START | GPIO33 STOP"));
}

void loop() {
    // Drena fila de log
    MensagemLog m;
    while (fila_log && xQueueReceive(fila_log, &m, 0) == pdTRUE) {
        Serial.println(m.texto);
    }
    processar_comandos_serial();
    processar_botoes();

    // Calibrações rodam aqui (blocking) com tasks pausadas via modo_ctrl.
    if (modo_app == MODO_CAL1) { cal1(); modo_app = MODO_EXEC; ativo = false; }
    else if (modo_app == MODO_CAL2) { cal2(); modo_app = MODO_EXEC; ativo = false; }
    else if (modo_app == MODO_CAL3) { cal3(); modo_app = MODO_EXEC; ativo = false; }

    delay(20);
}

#endif  // BUILD_POC
