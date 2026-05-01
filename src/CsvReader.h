// Streaming reader CSV linha-a-linha do LittleFS, sem alocar tudo em memoria.
// Schema esperado (gerado por _python/scripts/export_trajectory_csv.py):
//   idx,t_s,x_m,y_m,vel_mps,heading_x,heading_y,spreading,dose_alvo_kg_ha
//
// A coluna 'dose_alvo_kg_ha' eh opcional — se ausente, o consumidor pula a
// validacao cruzada Python <-> ESP32 e roda em modo standalone.
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include <cstdlib>
#include <cstring>

namespace vra {

struct LinhaCsv {
    long   idx;
    double t_s;
    double x_m;
    double y_m;
    double vel_mps;
    double heading_x;
    double heading_y;
    int    spreading;
    double dose_python;   // valido apenas se tem_dose_python = true
};

class CsvReader {
public:
    CsvReader() = default;
    ~CsvReader() { fechar(); }

    bool abrir(const char* path) {
        arquivo_ = LittleFS.open(path, "r");
        if (!arquivo_) return false;
        // Le a primeira linha (cabecalho) so para detectar a presenca da
        // coluna dose_alvo_kg_ha. As colunas obrigatorias sao assumidas
        // (idx,t_s,x_m,y_m,vel_mps,heading_x,heading_y,spreading,...).
        const String cab = arquivo_.readStringUntil('\n');
        tem_dose_python_ = cab.indexOf("dose_alvo_kg_ha") >= 0;
        n_linhas_ = 0;
        return true;
    }

    bool tem_dose_python() const { return tem_dose_python_; }
    long n_linhas_lidas() const { return n_linhas_; }

    bool proximaLinha(LinhaCsv& out) {
        if (!arquivo_ || !arquivo_.available()) return false;
        const String linha = arquivo_.readStringUntil('\n');
        if (linha.length() == 0) return false;

        // Copia para buffer mutavel para usar strtok_r.
        char buf[256];
        const size_t n = linha.length();
        if (n >= sizeof(buf)) return false;
        std::memcpy(buf, linha.c_str(), n);
        buf[n] = '\0';
        // remove \r residual no fim
        if (n > 0 && buf[n - 1] == '\r') buf[n - 1] = '\0';

        char* save = nullptr;
        char* tok;

        auto next_d = [&](double& dst) -> bool {
            tok = strtok_r(nullptr, ",", &save);
            if (!tok) return false;
            dst = std::strtod(tok, nullptr);
            return true;
        };
        auto next_l = [&](long& dst) -> bool {
            tok = strtok_r(nullptr, ",", &save);
            if (!tok) return false;
            dst = std::strtol(tok, nullptr, 10);
            return true;
        };

        // Primeira coluna: idx
        tok = strtok_r(buf, ",", &save);
        if (!tok) return false;
        out.idx = std::strtol(tok, nullptr, 10);

        if (!next_d(out.t_s))       return false;
        if (!next_d(out.x_m))       return false;
        if (!next_d(out.y_m))       return false;
        if (!next_d(out.vel_mps))   return false;
        if (!next_d(out.heading_x)) return false;
        if (!next_d(out.heading_y)) return false;

        long spread = 0;
        if (!next_l(spread)) return false;
        out.spreading = static_cast<int>(spread);

        if (tem_dose_python_) {
            if (!next_d(out.dose_python)) out.dose_python = 0.0;
        } else {
            out.dose_python = 0.0;
        }

        ++n_linhas_;
        return true;
    }

    // Variante que pula linhas com spreading=0 (curvas em U na cabeceira do
    // boustrofedon Python). Necessario porque o port simplificado da
    // Trajetoria no ESP32 nao gera arcos de curva — pula direto para a
    // proxima faixa. Sincroniza ambos os lados em fixes "espalhando".
    bool proximaLinhaComSpreading(LinhaCsv& out) {
        while (proximaLinha(out)) {
            if (out.spreading != 0) return true;
        }
        return false;
    }

    void fechar() {
        if (arquivo_) arquivo_.close();
        n_linhas_ = 0;
        tem_dose_python_ = false;
    }

private:
    File arquivo_;
    bool tem_dose_python_ = false;
    long n_linhas_ = 0;
};

}  // namespace vra
