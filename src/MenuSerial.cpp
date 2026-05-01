#include "MenuSerial.h"

#include <FS.h>
#include <LittleFS.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

#include "i18n.h"

namespace vra {

namespace {

bool terminaCom(const std::string& s, const char* sufixo) {
    const size_t n = std::strlen(sufixo);
    if (s.size() < n) return false;
    return s.compare(s.size() - n, n, sufixo) == 0;
}

std::string nomeBase(const std::string& kml) {
    // "ensaio_abcd.kml" -> "ensaio_abcd"; com path "/x.kml" -> "x"
    std::string s = kml;
    const size_t b = s.find_last_of('/');
    if (b != std::string::npos) s = s.substr(b + 1);
    const size_t p = s.find_last_of('.');
    if (p != std::string::npos) s = s.substr(0, p);
    return s;
}

}  // namespace

int MenuSerial::descobrir() {
    opcoes_.clear();
    // Pass unico: lista TODOS os arquivos do raiz, separa em KMLs +
    // conjunto de CSVs disponiveis. Evita LittleFS.exists() que loga
    // "[E][vfs_api] open(): ... does not exist" via printf direto (nao
    // capturado por esp_log_level_set).
    std::set<std::string> csvs;
    std::vector<std::string> kmls;

    File raiz = LittleFS.open("/");
    if (!raiz || !raiz.isDirectory()) return 0;
    File f = raiz.openNextFile();
    while (f) {
        std::string nome = f.name();
        if (!nome.empty() && nome[0] == '/') nome.erase(0, 1);
        if (terminaCom(nome, ".kml") || terminaCom(nome, ".KML")) {
            kmls.push_back(nome);
        } else if (terminaCom(nome, ".csv") || terminaCom(nome, ".CSV")) {
            csvs.insert(std::string("/") + nome);
        }
        f = raiz.openNextFile();
    }

    for (const auto& nome : kmls) {
        OpcaoKml op;
        op.nome = nome;
        op.caminho = std::string("/") + nome;
        const std::string base = nomeBase(nome);
        op.csv_caminho = std::string("/trajetoria_") + base + ".csv";
        op.tem_csv = csvs.count(op.csv_caminho) > 0;
        opcoes_.push_back(op);
    }
    std::sort(opcoes_.begin(), opcoes_.end(),
              [](const OpcaoKml& a, const OpcaoKml& b) { return a.nome < b.nome; });
    return static_cast<int>(opcoes_.size());
}

void MenuSerial::exibir(Print& out) const {
    out.println(MSG_KMLS_FOUND);
    char buf[160];
    for (size_t i = 0; i < opcoes_.size(); ++i) {
        const auto& op = opcoes_[i];
        std::snprintf(buf, sizeof(buf), "  %d. %-28s %s",
                      static_cast<int>(i + 1), op.nome.c_str(),
                      op.tem_csv ? MSG_CSV_AVAILABLE : MSG_CSV_MISSING);
        out.println(buf);
    }
}

int MenuSerial::lerEscolha(Stream& in, Print& out) const {
    const int n = static_cast<int>(opcoes_.size());
    if (n == 1) {
        char buf[120];
        std::snprintf(buf, sizeof(buf), "%s %s", opcoes_[0].nome.c_str(), MSG_SINGLE_KML);
        out.println(buf);
        return 0;
    }
    while (true) {
        char prompt[40];
        std::snprintf(prompt, sizeof(prompt), MSG_CHOOSE_PROMPT, n);
        out.print(prompt);

        // Bloqueia ate ter linha disponivel.
        while (!in.available()) { delay(20); }
        const String linha = in.readStringUntil('\n');
        out.println();
        const long val = std::strtol(linha.c_str(), nullptr, 10);
        if (val >= 1 && val <= n) {
            return static_cast<int>(val - 1);
        }
        out.println(MSG_INVALID_CHOICE);
    }
}

}  // namespace vra
