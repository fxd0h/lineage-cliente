// World-map items (S_UseMap, opcode USEMAP). Using a map item (ids 40373..40390) makes the server send
// the item objid and a map number (1..18). The 2009 US client (S3EP1U.bin) handles it at 0x48e0b0:
// it looks the item up in the inventory (0x44c110), stores `number - 1` in the map window object
// (global 0x816b08, class vtable 0x4f1aac) and opens it (0x4d0f30). The window (0x46ba90 draws it):
//  - the picture is Sprite.pak image table[number - 1] at 0x6de444:
//    1296 1305 1307 1309 1303 1312 1311 1302 1304 1295 1293 1306 1308 1310 1301 1294 1292 1417
//    (380x230 each, except 1294 = 1478x825 and 1301 = 500x320);
//  - there is no frame: the bare picture is clipped to a view through the mask "19.alp", at the
//    window object's position (starts at 0,0 of the game surface), dragged with the mouse (0x46afc0);
//  - the player marker is image 1297..1299 (animated every 250 ms), drawn only while the player is on
//    the map the picture shows, placed through per-map tables built at run time (not reproduced here);
//  - a right button double click inside the view closes it (0x46c1b0); the open windows are a stack.
// Here: the picture is centered on first show, a left drag moves the window (or pans a picture bigger
// than the view), right click or Escape closes it. The simple UI adds a framed title bar with [x].
#include <algorithm>
#include <cstdio>
#include "app/app.h"

namespace lin {

namespace {

constexpr int MAP_COUNT = 18;
// Sprite.pak image of each map number (index = number - 1), the table at 0x6de444 of the original.
constexpr u32 MAP_IMAGE[MAP_COUNT] = {1296, 1305, 1307, 1309, 1303, 1312, 1311, 1302, 1304,
                                      1295, 1293, 1306, 1308, 1310, 1301, 1294, 1292, 1417};
constexpr int FRAME_PAD = 8, TITLE_H = 22;

struct View {
    int x, y, w, h;  // the picture view, in logical pixels
};

}  // namespace

void eventos_mapa(App& a, const Evento& e) {
    if (e.tipo != Ev::USAR_MAPA) return;
    MapaItem& m = a.mapa_item;
    const int n = e.v[0];
    if (n < 1 || n > MAP_COUNT) return;
    m = {};
    m.numero = n;
    m.objeto = e.id;
}

static View view_of(const App& a, const RegionAtlas& r) {
    const MapaItem& m = a.mapa_item;
    const int margin = a.cfg.ui_simple ? FRAME_PAD : 0, top = a.cfg.ui_simple ? TITLE_H + FRAME_PAD : 0;
    const int w = std::min<int>(r.w, a.gpu.ancho() - 2 * margin - 4);
    const int h = std::min<int>(r.h, a.gpu.alto() - top - margin - 4);
    View v{m.x, m.y, w, h};
    if (m.x == INT32_MIN) {  // first frame: centered
        v.x = (a.gpu.ancho() - w) / 2;
        v.y = (a.gpu.alto() - h) / 2 + top / 2;
    }
    return v;
}

bool sobre_mapa(const App& a) {
    const MapaItem& m = a.mapa_item;
    if (!m.numero) return false;
    if (m.arrastrando) return true;
    const RegionAtlas& r = a.g->img(MAP_IMAGE[m.numero - 1]);
    if (!r.valida()) return false;
    const View v = view_of(a, r);
    const int pad = a.cfg.ui_simple ? FRAME_PAD : 0, top = a.cfg.ui_simple ? TITLE_H + FRAME_PAD : 0;
    return ui::sobre(a.entrada, v.x - pad, v.y - top, v.w + 2 * pad, v.h + top + pad);
}

void dibujar_mapa_item(App& a) {
    MapaItem& m = a.mapa_item;
    if (!m.numero) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const RegionAtlas full = g.img(MAP_IMAGE[m.numero - 1]);  // copy: other loads may move the cache
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 76), kf = clave_orden(CAPA_UI, 76);
    if (e.escape) {
        m = {};
        e.escape = false;
        return;
    }
    if (!full.valida()) return;  // still loading
    View v = view_of(a, full);
    if (m.x == INT32_MIN) {
        m.x = v.x;
        m.y = v.y;
        m.pan_x = (full.w - v.w) / 2;  // a picture bigger than the view starts centered
        m.pan_y = (full.h - v.h) / 2;
    }
    const bool simple = a.cfg.ui_simple;
    const int pad = simple ? FRAME_PAD : 0, top = simple ? TITLE_H + FRAME_PAD : 0;
    if (simple) {
        ui::recuadro(g, v.x - pad, v.y - top, v.w + 2 * pad, v.h + top + pad, 245);
        char txt[48];
        std::snprintf(txt, sizeof txt, "Map %d", m.numero);
        g.texto(txt, static_cast<float>(v.x), static_cast<float>(v.y - top + 5), kt, ui::COLOR_TITULO);
        const int cx = v.x + v.w - 22, cy = v.y - top + 3;
        g.texto("[x]", static_cast<float>(cx), static_cast<float>(cy + 2), kt, ui::COLOR_TEXTO);
        if (e.clic && ui::sobre(e, cx - 2, cy, 26, 18)) {
            m = {};
            return;
        }
    }
    const bool over = ui::sobre(e, v.x, v.y, v.w, v.h);
    if (over && e.clic_derecho) {  // the original closes with a right button double click
        m = {};
        return;
    }
    // Drag: moves the window, or pans the picture when it is bigger than the view.
    const bool bigger = full.w > v.w || full.h > v.h;
    if (e.pulsado && (over || (simple && ui::sobre(e, v.x - pad, v.y - top, v.w + 2 * pad, top)))) {
        m.arrastrando = true;
        m.ult_x = e.mx;
        m.ult_y = e.my;
    }
    if (m.arrastrando && e.presionado) {
        const int dx = e.mx - m.ult_x, dy = e.my - m.ult_y;
        if (bigger && over) {
            m.pan_x -= dx;
            m.pan_y -= dy;
        } else {
            m.x += dx;
            m.y += dy;
        }
        m.ult_x = e.mx;
        m.ult_y = e.my;
    }
    if (!e.presionado) m.arrastrando = false;
    m.x = std::clamp(m.x, -v.w + 40, a.gpu.ancho() - 40);
    m.y = std::clamp(m.y, 0, a.gpu.alto() - 20);
    m.pan_x = std::clamp(m.pan_x, 0, std::max(0, full.w - v.w));
    m.pan_y = std::clamp(m.pan_y, 0, std::max(0, full.h - v.h));
    v.x = m.x;
    v.y = m.y;
    // The visible part of the picture: a sub-rectangle of the atlas region.
    RegionAtlas part = full;
    const u32 k = full.escala_texel();
    part.x = static_cast<u16>(full.x + static_cast<u32>(m.pan_x) * k);
    part.y = static_cast<u16>(full.y + static_cast<u32>(m.pan_y) * k);
    part.w = static_cast<u16>(v.w);
    part.h = static_cast<u16>(v.h);
    part.ox = part.oy = 0;
    part.aura_dx = 0;
    g.gpu().dibujar(part, v.x, v.y, kf + 1);
}

}  // namespace lin
