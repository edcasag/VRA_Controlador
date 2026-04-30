// Kml — parser do mapa de aplicação conforme Tabela 4 do Cap. 6 da tese.
// Categorias: Talhao (perímetro externo + dose-base), RegiaoPoligonal
// (inclusão dose>0 ou exclusão dose==0), RegiaoCircular (label+raio),
// PontoReferencia (lat/lon+dose, usado pelo IDW).
#pragma once

#include <string>
#include <vector>

#include "Geometria.h"

class Print;

namespace tinyxml2 {
class XMLElement;
}

namespace vra {

struct Talhao {
    double dose_base = 0.0;            // kg/ha
    std::vector<Ponto> vertices;
    double area_m2 = 0.0;
};

struct RegiaoPoligonal {
    std::string label;
    double dose = 0.0;                 // kg/ha (0 = exclusão)
    std::vector<Ponto> vertices;
    double area_m2 = 0.0;
};

struct RegiaoCircular {
    std::string label;
    double dose = 0.0;                 // kg/ha (0 = exclusão)
    double raio_m = 0.0;
    Ponto centro {0.0, 0.0};
};

struct PontoReferencia {
    std::string label;
    double dose = 0.0;                 // kg/ha
    Ponto pos {0.0, 0.0};
};

class Kml {
public:
    bool carregarDoArquivo(const char* caminho);
    bool carregarDoTexto(const char* xml);

    bool   talhaoCarregado()                                   const { return talhao_carregado_; }
    const  Talhao&                       talhao()              const { return talhao_; }
    const  std::vector<RegiaoPoligonal>& regioesPoligonais()   const { return regioes_poly_; }
    const  std::vector<RegiaoCircular>&  regioesCirculares()   const { return regioes_circ_; }
    const  std::vector<PontoReferencia>& pontosReferencia()    const { return pontos_; }
    Coord                                origem()              const { return origem_; }

    void imprimirSumario(Print& out) const;
    void imprimirCsv(Print& out)     const;

private:
    Talhao                       talhao_;
    bool                         talhao_carregado_ = false;
    std::vector<RegiaoPoligonal> regioes_poly_;
    std::vector<RegiaoCircular>  regioes_circ_;
    std::vector<PontoReferencia> pontos_;
    Coord                        origem_ {0.0, 0.0};

    bool descobrirOrigem(tinyxml2::XMLElement* root);
    void processarPlacemarks(tinyxml2::XMLElement* root);
};

}  // namespace vra
