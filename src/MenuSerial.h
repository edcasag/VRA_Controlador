// Menu interativo via Serial: lista *.kml em / do LittleFS, exibe opcoes
// numeradas e le a escolha do usuario. Quando ha 1 KML so, carrega direto.
// Auto-deteccao de CSV ground truth por convencao de nome:
//   ensaio_abcd.kml -> trajetoria_ensaio_abcd.csv
#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

namespace vra {

struct OpcaoKml {
    std::string nome;          // ex: "ensaio_abcd.kml"
    std::string caminho;       // ex: "/ensaio_abcd.kml"
    std::string csv_caminho;   // ex: "/trajetoria_ensaio_abcd.csv" (vazio se nao existe)
    bool        tem_csv;
};

class MenuSerial {
public:
    // Itera LittleFS e enche opcoes_ com todos os *.kml encontrados em /.
    // Retorna numero de KMLs encontrados.
    int descobrir();

    // Imprime menu numerado em out (usa macros do i18n.h).
    void exibir(Print& out) const;

    // Le escolha do usuario via in (bloqueia ate ter input valido).
    // Retorna indice (0..n-1) na opcoes_. Se ha 1 so, retorna 0 sem prompt.
    int lerEscolha(Stream& in, Print& out) const;

    const std::vector<OpcaoKml>& opcoes() const { return opcoes_; }

private:
    std::vector<OpcaoKml> opcoes_;
};

}  // namespace vra
