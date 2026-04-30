#include "Kml.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Print.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tinyxml2.h"

namespace vra {

namespace {

// Resultado da classificação do <name> de um Placemark, conforme Tab. 4
// (artigo SBIAGRO 2025).
enum class TipoFeature { Invalido, Field, Label, RateOnly, Circle, CircleSemLabel };
struct NomeClassificado {
    TipoFeature tipo = TipoFeature::Invalido;
    std::string label;
    double      dose   = 0.0;
    double      raio_m = 0.0;
};

void rtrimMetros(std::string& s) {
    while (!s.empty() && (s.back() == 'm' || s.back() == 'M' || std::isspace(static_cast<unsigned char>(s.back()))))
        s.pop_back();
}

std::string trimCopia(const char* texto) {
    if (!texto) return {};
    std::string s = texto;
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
    return s;
}

bool parseDouble(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double x = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (end && *end != '\0') return false;
    v = x;
    return true;
}

NomeClassificado classificarNome(const char* texto_bruto) {
    NomeClassificado r;
    std::string s = trimCopia(texto_bruto);
    if (s.empty()) return r;

    const auto pos_eq = s.find('=');
    const auto pos_dp = s.find(':');

    // "Label=Rate:Radius" ou "Label=Rate:Radius m"
    if (pos_eq != std::string::npos && pos_dp != std::string::npos && pos_dp > pos_eq) {
        std::string label  = trimCopia(s.substr(0, pos_eq).c_str());
        std::string sdose  = trimCopia(s.substr(pos_eq + 1, pos_dp - pos_eq - 1).c_str());
        std::string sraio  = trimCopia(s.substr(pos_dp + 1).c_str());
        rtrimMetros(sraio);
        double dose = 0.0, raio = 0.0;
        if (!parseDouble(sdose, dose) || !parseDouble(sraio, raio)) return r;
        r.tipo   = TipoFeature::Circle;
        r.label  = std::move(label);
        r.dose   = dose;
        r.raio_m = raio;
        return r;
    }

    // "Rate:Radius" ou "Rate:Radius m"
    if (pos_eq == std::string::npos && pos_dp != std::string::npos) {
        std::string sdose = trimCopia(s.substr(0, pos_dp).c_str());
        std::string sraio = trimCopia(s.substr(pos_dp + 1).c_str());
        rtrimMetros(sraio);
        double dose = 0.0, raio = 0.0;
        if (!parseDouble(sdose, dose) || !parseDouble(sraio, raio)) return r;
        r.tipo   = TipoFeature::CircleSemLabel;
        r.dose   = dose;
        r.raio_m = raio;
        return r;
    }

    // "Label=Rate"
    if (pos_eq != std::string::npos) {
        std::string label = trimCopia(s.substr(0, pos_eq).c_str());
        std::string sdose = trimCopia(s.substr(pos_eq + 1).c_str());
        double dose = 0.0;
        if (!parseDouble(sdose, dose)) return r;
        std::string label_lower = label;
        for (auto& c : label_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (label_lower == "field") {
            r.tipo  = TipoFeature::Field;
            r.label = std::move(label);
            r.dose  = dose;
        } else {
            r.tipo  = TipoFeature::Label;
            r.label = std::move(label);
            r.dose  = dose;
        }
        return r;
    }

    // "Rate" puro
    double dose = 0.0;
    if (parseDouble(s, dose)) {
        r.tipo = TipoFeature::RateOnly;
        r.dose = dose;
        return r;
    }

    return r;  // Invalido
}

// Lê o texto de "<coordinates>lon,lat,alt lon,lat,alt ...</coordinates>"
// e devolve a lista projetada para o sistema local da origem informada.
std::vector<Ponto> parseCoordsList(const char* texto, Coord origem) {
    std::vector<Ponto> saida;
    if (!texto) return saida;
    const char* p = texto;
    while (*p) {
        while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
        if (!*p) break;
        char* end = nullptr;
        const double lon = std::strtod(p, &end);
        if (!end || end == p || *end != ',') break;
        p = end + 1;
        const double lat = std::strtod(p, &end);
        if (!end || end == p) break;
        // descarta resto até espaço (altitude opcional)
        p = end;
        while (*p && !std::isspace(static_cast<unsigned char>(*p))) ++p;
        saida.push_back(projetar(Coord{lat, lon}, origem));
    }
    return saida;
}

// Devolve (lat, lon) do primeiro par de "<coordinates>lon,lat,alt ...</coordinates>".
bool primeiroLatLon(const char* texto, double& lat, double& lon) {
    if (!texto) return false;
    const char* p = texto;
    while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    char* end = nullptr;
    const double lon_v = std::strtod(p, &end);
    if (!end || end == p || *end != ',') return false;
    p = end + 1;
    const double lat_v = std::strtod(p, &end);
    if (!end || end == p) return false;
    lat = lat_v;
    lon = lon_v;
    return true;
}

// Acha um descendente com nome 'tag', percorrendo recursivamente.
tinyxml2::XMLElement* acharDescendente(tinyxml2::XMLElement* el, const char* tag) {
    if (!el) return nullptr;
    if (std::strcmp(el->Name(), tag) == 0) return el;
    for (auto* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        if (auto* hit = acharDescendente(c, tag)) return hit;
    }
    return nullptr;
}

void coletarPlacemarks(tinyxml2::XMLElement* el,
                       std::vector<tinyxml2::XMLElement*>& saida) {
    if (!el) return;
    for (auto* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        if (std::strcmp(c->Name(), "Placemark") == 0) saida.push_back(c);
        else                                          coletarPlacemarks(c, saida);
    }
}

const char* textoFilho(tinyxml2::XMLElement* el, const char* tag) {
    if (!el) return nullptr;
    auto* hit = acharDescendente(el, tag);
    return hit ? hit->GetText() : nullptr;
}

void escreverLinha(Print& out, const char* fmt, ...) {
    char buf[200];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    out.println(buf);
}

}  // namespace

bool Kml::descobrirOrigem(tinyxml2::XMLElement* root) {
    std::vector<tinyxml2::XMLElement*> placemarks;
    coletarPlacemarks(root, placemarks);
    for (auto* pm : placemarks) {
        const char* coords = textoFilho(pm, "coordinates");
        if (!coords) continue;
        double lat = 0.0, lon = 0.0;
        if (primeiroLatLon(coords, lat, lon)) {
            origem_ = Coord{lat, lon};
            return true;
        }
    }
    return false;
}

void Kml::processarPlacemarks(tinyxml2::XMLElement* root) {
    std::vector<tinyxml2::XMLElement*> placemarks;
    coletarPlacemarks(root, placemarks);

    for (auto* pm : placemarks) {
        auto* nome_el = pm->FirstChildElement("name");
        const char* nome_txt = nome_el ? nome_el->GetText() : nullptr;
        const NomeClassificado cls = classificarNome(nome_txt);

        auto* poly_coords  = acharDescendente(pm, "Polygon");
        auto* point_coords = acharDescendente(pm, "Point");
        const char* poly_txt  = poly_coords  ? textoFilho(poly_coords,  "coordinates") : nullptr;
        const char* point_txt = point_coords ? textoFilho(point_coords, "coordinates") : nullptr;

        // Polígono sem tag de dose: trata como contorno do talhão (dose=0)
        // se ainda não houver Field= explícito.
        if (cls.tipo == TipoFeature::Invalido) {
            if (poly_txt && !talhao_carregado_) {
                auto pts = parseCoordsList(poly_txt, origem_);
                talhao_.dose_base = 0.0;
                talhao_.vertices  = std::move(pts);
                talhao_.area_m2   = polygonArea(talhao_.vertices);
                talhao_carregado_ = true;
            }
            continue;
        }

        if (poly_txt) {
            auto pts = parseCoordsList(poly_txt, origem_);
            if (cls.tipo == TipoFeature::Field) {
                talhao_.dose_base = cls.dose;
                talhao_.vertices  = std::move(pts);
                talhao_.area_m2   = polygonArea(talhao_.vertices);
                talhao_carregado_ = true;
            } else if (cls.tipo == TipoFeature::Label || cls.tipo == TipoFeature::RateOnly) {
                RegiaoPoligonal rp;
                rp.label    = cls.label;
                rp.dose     = cls.dose;
                rp.vertices = std::move(pts);
                rp.area_m2  = polygonArea(rp.vertices);
                regioes_poly_.push_back(std::move(rp));
            }
        } else if (point_txt) {
            double lat = 0.0, lon = 0.0;
            if (!primeiroLatLon(point_txt, lat, lon)) continue;
            const Ponto p = projetar(Coord{lat, lon}, origem_);
            if (cls.tipo == TipoFeature::Circle || cls.tipo == TipoFeature::CircleSemLabel) {
                RegiaoCircular rc;
                rc.label  = cls.label;
                rc.dose   = cls.dose;
                rc.raio_m = cls.raio_m;
                rc.centro = p;
                regioes_circ_.push_back(std::move(rc));
            } else {
                PontoReferencia pr;
                pr.label = cls.label;
                pr.dose  = cls.dose;
                pr.pos   = p;
                pontos_.push_back(std::move(pr));
            }
        }
    }
}

bool Kml::carregarDoArquivo(const char* caminho) {
    fs::File f = LittleFS.open(caminho, "r");
    if (!f) return false;
    const size_t n = f.size();
    std::string buf;
    buf.resize(n);
    f.readBytes(&buf[0], n);
    f.close();
    return carregarDoTexto(buf.c_str());
}

bool Kml::carregarDoTexto(const char* xml) {
    talhao_           = Talhao{};
    talhao_carregado_ = false;
    regioes_poly_.clear();
    regioes_circ_.clear();
    pontos_.clear();
    origem_ = Coord{0.0, 0.0};

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml) != tinyxml2::XML_SUCCESS) return false;
    auto* root = doc.RootElement();
    if (!root) return false;

    if (!descobrirOrigem(root)) return false;
    processarPlacemarks(root);
    return true;
}

void Kml::imprimirSumario(Print& out) const {
    out.println(F("=== KML SUMARIO ==="));
    escreverLinha(out, "Origem: lat=%.7f lon=%.7f", origem_.lat, origem_.lon);
    if (talhao_carregado_) {
        escreverLinha(out, "Talhao: %u vertices, area = %.2f ha, dose-base = %.0f kg/ha",
                      static_cast<unsigned>(talhao_.vertices.size()),
                      talhao_.area_m2 / 10000.0, talhao_.dose_base);
    } else {
        out.println(F("Talhao: (nao carregado)"));
    }

    size_t n_inc = 0, n_exc = 0;
    for (const auto& r : regioes_poly_) (r.dose > 0 ? n_inc : n_exc)++;
    escreverLinha(out, "Regioes poligonais: %u inclusao + %u exclusao",
                  static_cast<unsigned>(n_inc), static_cast<unsigned>(n_exc));
    for (const auto& r : regioes_poly_) {
        escreverLinha(out, "  - %-12s dose=%-5.0f area=%.2f ha (%u vert.)",
                      r.label.c_str(), r.dose, r.area_m2 / 10000.0,
                      static_cast<unsigned>(r.vertices.size()));
    }

    size_t n_circ_inc = 0, n_circ_exc = 0;
    for (const auto& c : regioes_circ_) (c.dose > 0 ? n_circ_inc : n_circ_exc)++;
    escreverLinha(out, "Regioes circulares: %u (inc=%u, exc=%u)",
                  static_cast<unsigned>(regioes_circ_.size()),
                  static_cast<unsigned>(n_circ_inc),
                  static_cast<unsigned>(n_circ_exc));
    for (const auto& c : regioes_circ_) {
        escreverLinha(out, "  - %-12s dose=%-5.0f raio=%.1f m  centro=(%.1f, %.1f)",
                      c.label.c_str(), c.dose, c.raio_m, c.centro.x, c.centro.y);
    }

    escreverLinha(out, "Pontos de referencia: %u",
                  static_cast<unsigned>(pontos_.size()));
    for (const auto& p : pontos_) {
        escreverLinha(out, "  - %-12s dose=%-5.0f pos=(%.1f, %.1f)",
                      p.label.c_str(), p.dose, p.pos.x, p.pos.y);
    }
}

void Kml::imprimirCsv(Print& out) const {
    out.println(F("=== CSV BEGIN ==="));
    out.println(F("tipo,label,dose_kg_ha,raio_m,vertices_ou_x,area_ha_ou_y"));
    if (talhao_carregado_) {
        escreverLinha(out, "talhao,Field,%.0f,,%u,%.4f",
                      talhao_.dose_base,
                      static_cast<unsigned>(talhao_.vertices.size()),
                      talhao_.area_m2 / 10000.0);
    }
    for (const auto& r : regioes_poly_) {
        escreverLinha(out, "poligono,%s,%.0f,,%u,%.4f",
                      r.label.c_str(), r.dose,
                      static_cast<unsigned>(r.vertices.size()),
                      r.area_m2 / 10000.0);
    }
    for (const auto& c : regioes_circ_) {
        escreverLinha(out, "circular,%s,%.0f,%.2f,%.2f,%.2f",
                      c.label.c_str(), c.dose, c.raio_m, c.centro.x, c.centro.y);
    }
    for (const auto& p : pontos_) {
        escreverLinha(out, "ponto,%s,%.0f,,%.2f,%.2f",
                      p.label.c_str(), p.dose, p.pos.x, p.pos.y);
    }
    out.println(F("=== CSV END ==="));
}

bool Kml::bbox(double& xmin, double& ymin, double& xmax, double& ymax) const {
    xmin = ymin = +1e18;
    xmax = ymax = -1e18;
    bool tem = false;
    auto incluir = [&](double x, double y) {
        if (x < xmin) xmin = x;
        if (x > xmax) xmax = x;
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
        tem = true;
    };
    if (talhao_carregado_) {
        for (const auto& v : talhao_.vertices) incluir(v.x, v.y);
    }
    for (const auto& r : regioes_poly_) {
        for (const auto& v : r.vertices) incluir(v.x, v.y);
    }
    for (const auto& c : regioes_circ_) incluir(c.centro.x, c.centro.y);
    for (const auto& p : pontos_)        incluir(p.pos.x,    p.pos.y);
    return tem;
}

}  // namespace vra
