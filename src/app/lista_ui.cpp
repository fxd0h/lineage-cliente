// Original party / pledge / buddy list windows (original UI only), opened by the server's S_ShowHTML pages "party",
// "pledge" and "buddy" instead of showing them as HTML (windows 0x497240 / 0x4b92f0 / 0x419040 of S3EP1U.bin, all of
// the same shape; global [0x82468c] for the party one):
//  - translucent background 978 at (8,11), frame 979 (the 980 art of the audit is its overlay), close 969, scroll bar
//    with track 971, arrows 974 / 972 and thumb 976 at x = frame width - 31, y 47 (0x47cd80);
//  - up to 17 rows (0x497184: `edi < first + 0x11`) from (25, 49), one every row height (13 px, the font height), each a
//    name; a flagged row (the member is online) is tinted by the table blend 0x498d90 over 145 px, colour [0x8f2b3c]
//    (the fork: a translucent blue).
// Page data (S_Party, S_Pledge, S_Buddy; fields separated by SEPARATOR_DATOS): party = name, members; pledge = clan, online,
// all (type 1 sends only the online list in the third field); buddy = list, online list. Names are space separated.
// Unverified: the exact scroll bar and title positions, which flag the exe tints, the row height.
#include <algorithm>
#include <sstream>
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 IMG_BG = 978, IMG_FRAME = 979;
constexpr int ROW_H = 13, VISIBLE = 17, ROW_X = 25, ROW_Y = 49, SCROLL_Y = 47, TITLE_Y = 28;

std::vector<std::string> split(std::string_view s, char sep) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i <= s.size()) {
        const size_t j = std::min(s.find(sep, i), s.size());
        if (j > i) out.emplace_back(s.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

}  // namespace

bool es_pagina_lista(std::string_view p) { return p == "party" || p == "pledge" || p == "buddy"; }

void abrir_lista_orig(App& a, std::string_view pagina, std::string_view datos) {
    auto& l = a.lista;
    const std::vector<std::string> campo = split(datos, SEPARADOR_DATOS);
    auto nombres = [&](size_t i) { return i < campo.size() ? split(campo[i], ' ') : std::vector<std::string>{}; };
    l = {};
    l.abierta = true;
    l.filas.clear();
    if (pagina == "party") {
        l.tipo = 0;
        l.titulo = campo.empty() ? "" : campo[0];
        for (std::string& n : nombres(1)) l.filas.emplace_back(std::move(n), false);
    } else if (pagina == "pledge") {
        l.tipo = 1;
        l.titulo = campo.empty() ? "" : campo[0];
        const std::vector<std::string> en_linea = nombres(1), todos = nombres(2);
        const bool solo_en_linea = en_linea.empty();  // type 1 of S_Pledge: only the online members, in the third field
        for (const std::string& n : todos)
            l.filas.emplace_back(n, solo_en_linea || std::find(en_linea.begin(), en_linea.end(), n) != en_linea.end());
    } else {
        l.tipo = 2;
        const std::vector<std::string> en_linea = nombres(1);
        for (const std::string& n : nombres(0)) l.filas.emplace_back(n, std::find(en_linea.begin(), en_linea.end(), n) != en_linea.end());
    }
}

Rect rect_lista_orig(const App& a) {
    const RegionAtlas f = a.g ? a.g->img(IMG_FRAME) : RegionAtlas{};
    return {(a.gpu.ancho() - f.w) / 2 - 130, 40, f.w, f.h};
}

void dibujar_lista_orig(App& a) {
    if (!a.lista.abierta || a.cfg.ui_simple) return;
    auto& l = a.lista;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::LISTA);
    const Rect r = rect_ventana(a, IdVentana::LISTA);
    if (draw_original_frame(a, r.x, r.y, IMG_BG, 8, 11, IMG_FRAME, r.w - 26, 6) || e.escape) {
        l.abierta = false;
        e.escape = false;
        return;
    }
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 16), kr = clave_orden(CAPA_UI, 12);
    if (!l.titulo.empty()) g.texto_sombra(l.titulo, static_cast<float>(r.x + ROW_X), static_cast<float>(r.y + TITLE_Y), kt, rgba(255, 230, 120));
    const u32 total = static_cast<u32>(l.filas.size());
    original_scrollbar(a, l.barra, r.x + r.w - 31, r.y + SCROLL_Y, VISIBLE * ROW_H, total, VISIBLE, l.pos, sobre_ventana(a, IdVentana::LISTA), 0, 0);
    for (u32 k = 0; k < VISIBLE && l.pos + k < total; ++k) {
        const auto& [nombre, en_linea] = l.filas[l.pos + k];
        const int y = r.y + ROW_Y + static_cast<int>(k) * ROW_H;
        if (en_linea) g.gpu().rectangulo(r.x + ROW_X, y, 145, ROW_H, kr, rgba(60, 90, 170, 110));
        g.texto_sombra(nombre, static_cast<float>(r.x + ROW_X + 2), static_cast<float>(y + (ROW_H - ALTO_GLIFO) / 2), kt, rgba(255, 255, 254));
    }
}

}  // namespace lin
